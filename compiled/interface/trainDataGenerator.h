/*
 * trainDataGenerator.h
 *
 *  Created on: 7 Nov 2019
 *      Author: jkiesele
 */

#ifndef DJCDEV_DEEPJETCORE_COMPILED_INTERFACE_TRAINDATAGENERATOR_H_
#define DJCDEV_DEEPJETCORE_COMPILED_INTERFACE_TRAINDATAGENERATOR_H_

#ifdef DJC_DATASTRUCTURE_PYTHON_BINDINGS
#include <boost/python.hpp>
#include "boost/python/numpy.hpp"
#include "boost/python/list.hpp"
#include <boost/python/exception_translator.hpp>
#include "helper.h"
#include "pythonToSTL.h"
#endif

#include <string>
#include <vector>
#include "trainData.h"
#include <algorithm>
#include <random>
#include <iterator>
#include <thread>
#include <iostream>

namespace djc{

/*
 * Base class, no numpy interface or anything yet.
 * Inherit from/use this class and define the actual batch feed function.
 * This could as well be filling a (ragged) tensorflow tensor
 *
 *
 * Notes for future improvements:
 *
 *  - pre-split trainData in buffer (just make it a vector/fifo-like queue)
 *    propagates to trainData, simpleArray, then make multiple memcpy (even threaded?)
 *    (but not read - it is still a split!)
 *    This makes the second thread obsolete, and still everything way faster!
 *
 *  - for ragged: instead of batch size, set upper limit on data size (number of floats)
 *    can be used to pre-split in a similar way
 *
 *
 *
 *
 */
template <class T>
class trainDataGenerator{
public:
    trainDataGenerator();
    ~trainDataGenerator();

    /**
     * Also opens all files (verify) and gets the total sample size
     */
    void setFileList(const std::vector<std::string>& files){
        clear();
        orig_infiles_=files;
        shuffle_indices_.resize(orig_infiles_.size());
        for(size_t i=0;i<shuffle_indices_.size();i++)
            shuffle_indices_[i]=i;
        readInfo();
    }
    void setBuffer(const trainData<T>&);

    void setBatchSize(size_t nelements){
        batchsize_= nelements;
        if(orig_rowsplits_.size())
            prepareSplitting();
    }
    void setSquaredElementsLimit(bool use_sq_limit){
        sqelementslimit_=use_sq_limit;
        if(orig_rowsplits_.size())
            prepareSplitting();
    }
    void setSkipTooLargeBatches(bool skipthem){
        skiplargebatches_=skipthem;
        if(orig_rowsplits_.size())
            prepareSplitting();
    }

    int getNTotal()const{return ntotal_;}

    void setFileTimeout(size_t seconds){
        filetimeout_=seconds;
    }

    int getNBatches()const{return nbatches_;}

    bool lastBatch()const;

    void prepareNextEpoch();

    void shuffleFilelist();

    void end();
    /**
     * clears all dataset related info but keeps batch size, file timout etc
     */
    void clear();

    /**
     * gets Batch. If batchsize is specified, it is up to the user
     * to make sure that the sum of all batches is smaller or equal the
     * total sample size.
     * The batch size is always the size of the NEXT batch!
     *
     */
    trainData<T> getBatch(); //if no threading batch index can be given? just for future?

    bool debug;

#ifdef DJC_DATASTRUCTURE_PYTHON_BINDINGS
    void setFileListP(boost::python::list files){
        djc::trainDataGenerator<T>::setFileList(toSTLVector<std::string>(files));
    }
#endif


private:
    void readBuffer();
    void readInfo();
    void prepareSplitting();
    bool tdHasRaggedDimension(const trainData<T>& )const;

    trainData<T>  prepareBatch();
    std::vector<std::string> orig_infiles_;
    std::vector<size_t> shuffle_indices_;
    std::vector<std::vector<int64_t> > orig_rowsplits_;
    std::vector<size_t> splits_;
    std::vector<bool> usebatch_;
    int randomcount_;
    size_t batchsize_;
    bool sqelementslimit_,skiplargebatches_;

    trainData<T> buffer_store, buffer_read;
    std::thread * readthread_;
    std::string nextread_;
    size_t filecount_;
    size_t nbatches_;
    size_t ntotal_;
    size_t nsamplesprocessed_;
    size_t lastbatchsize_;
    size_t filetimeout_;
    size_t batchcount_;

};


template<class T>
trainDataGenerator<T>::trainDataGenerator() :debug(false),
        randomcount_(1), batchsize_(2),sqelementslimit_(false),skiplargebatches_(true), readthread_(0), filecount_(0), nbatches_(
                0), ntotal_(0), nsamplesprocessed_(0),lastbatchsize_(0),filetimeout_(10),
                batchcount_(0){
}

template<class T>
trainDataGenerator<T>::~trainDataGenerator(){
    if(readthread_){
        readthread_->join();
        delete readthread_;
    }

}

template<class T>
void trainDataGenerator<T>::shuffleFilelist(){
    std::random_device rd;
    std::mt19937 g(rd());
    g.seed(randomcount_);
    randomcount_++;
    std::shuffle(std::begin(shuffle_indices_),std::end(shuffle_indices_),g);

    //redo splits etc
    prepareSplitting();
    batchcount_=0;
}

template<class T>
void trainDataGenerator<T>::setBuffer(const trainData<T>& td){

    clear();
    if(td.featureShapes().size()<1 || td.featureShapes().at(0).size()<1)
        throw std::runtime_error("trainDataGenerator<T>::setBuffer: no features filled in trainData object");
    auto hasRagged = tdHasRaggedDimension(td);

    auto rs = td.getFirstRowsplits();
    if(rs.size())
        orig_rowsplits_.push_back(rs);
    shuffle_indices_.push_back(0);
    ntotal_ = td.nElements();
    buffer_store=td;
    prepareSplitting();

}

template<class T>
void trainDataGenerator<T>::readBuffer(){
    size_t ntries = 0;
    std::exception caught;
    while(ntries < filetimeout_){
        if(io::fileExists(nextread_)){
            try{
                buffer_read.readFromFile(nextread_);
                return;
            }
            catch(std::exception & e){ //if there are data glitches we don't want the whole training fail immediately
                caught=e;
                std::cout << "File not "<< nextread_ <<" successfully read: " << e.what() << std::endl;
                std::cout << "trying " << filetimeout_-ntries << " more time(s)" << std::endl;
                ntries+=1;
            }
        }
        sleep(1);
        ntries++;
    }
    buffer_read.clear();
    throw std::runtime_error("trainDataGenerator<T>::readBuffer: file "+nextread_+ " could not be read.");
}


template<class T>
void trainDataGenerator<T>::readInfo(){
    ntotal_=0;
    bool hasRagged=false;
    bool firstfile=true;
    for(const auto& f: orig_infiles_){
        trainData<T> td;

        td.readShapesFromFile(f);
        //first dimension is always Nelements. At least features are filled
        if(td.featureShapes().size()<1 || td.featureShapes().at(0).size()<1)
            throw std::runtime_error("trainDataGenerator<T>::readNTotal: no features filled in trainData object "+f);

        if(firstfile){
            hasRagged = tdHasRaggedDimension(td);
        }
        if(hasRagged){
            std::vector<int64_t> rowsplits = td.readShapesAndRowSplitsFromFile(f, firstfile);//check consistency only for first
            if(debug)
                std::cout << "rowsplits.size() " <<rowsplits.size() << ": "<<f <<  std::endl; //DEBUG
            orig_rowsplits_.push_back(rowsplits);
        }
        firstfile=false;
        ntotal_ += td.nElements();
    }
    if(debug)
        std::cout << "total elements "<< ntotal_ <<std::endl;
    batchcount_=0;
    prepareSplitting();
}


template<class T>
void trainDataGenerator<T>::prepareSplitting(){
    splits_.clear();
    nbatches_=0;
    if(orig_rowsplits_.size()<1){//no row splits, just equal batch size except for last batch
        size_t used_events=0;
        while(used_events<ntotal_){
            if(used_events + batchsize_ <= ntotal_){
                splits_.push_back(batchsize_);
                used_events+=batchsize_;
                nbatches_++;
            }
            else{
                splits_.push_back(ntotal_-used_events);
                nbatches_++;
                break;
            }
        }
        if(debug){
            std::cout << "trainDataGenerator<T>::prepareSplitting: splits" <<std::endl;
            for(const auto& s: splits_)
                std::cout << s << ", ";
            std::cout << std::endl;
        }
        return;
    }

    ///////row splits part

    std::vector<int64_t> allrs;
    for(size_t i=0;i<orig_rowsplits_.size();i++){
        const auto& thisrs = orig_rowsplits_.at(shuffle_indices_.at(i));
        if(i==0 || allrs.size()==0){
            allrs=thisrs;}
        else{
            size_t lastelemidx = allrs.size()-1;
            size_t lastnelements = allrs.at(lastelemidx);
            allrs.resize(lastelemidx+thisrs.size());
            for(size_t j=0;j<thisrs.size();j++)
                allrs.at(lastelemidx+j) = lastnelements+thisrs.at(j);
        }
    }

    if(debug){
        std::cout << "all (first 100) row splits " <<  allrs.size() << std::endl;
        int counter =0;
        for(const auto& s: allrs){
            std::cout << s << ", " ;
            if(counter>100)break;
            counter++;
        }
        std::cout << std::endl;
    }

    int debugcounter = 0;

    std::vector<size_t> batchlengths;
    size_t startnextat=0;
    while(startnextat < allrs.size()-1){
        bool exceeds=true;

        size_t initialpoint = startnextat;
        size_t batchlength = simpleArray<T>::findElementSplitLength(allrs, batchsize_, startnextat,exceeds, sqelementslimit_);
        size_t splitpoint = startnextat - initialpoint ;//since it will have been split off before

        if(!skiplargebatches_)
            exceeds=false;

        splits_.push_back(splitpoint);
        batchlengths.push_back(batchlength);
        usebatch_.push_back(!exceeds);

        if(debug && debugcounter<100)
            std::cout << ">>>> batch with size " << std::sqrt((float)batchlength) << " use " << !exceeds << " next start "<< startnextat<< " splitpoint "<<splitpoint <<" nelements " << allrs.at(startnextat)-allrs.at(initialpoint) << std::endl;

        if(!exceeds)
            nbatches_++;

        debugcounter++;
    }
    if(debug)
        std::cout << "prepared " << nbatches_ << " batches" << std::endl;

    if(debug){
        size_t nprint = batchlengths.size();
        if(nprint>200)nprint=200;
        for(size_t i=0;i< nprint;i++){
            std::cout << batchlengths.at(i) ;
            if(usebatch_.at(i))
                std::cout << " ok, split " ;
            else
                std::cout << " no, split ";
            std::cout << splits_.at(i) << "; ";
        }
        std::cout << std::endl;
    }

}

template<class T>
bool trainDataGenerator<T>::tdHasRaggedDimension(const trainData<T>& td)const{
    for(const auto& sv: td.featureShapes())
        for(const auto& s:sv)
            if(s<0)
                return true;
    for(const auto& sv: td.truthShapes())
        for(const auto& s:sv)
            if(s<0)
                return true;
    for(const auto& sv: td.weightShapes())
        for(const auto& s:sv)
            if(s<0)
                return true;
    return false;
}


template<class T>
bool trainDataGenerator<T>::lastBatch()const{
    return batchcount_ >= getNBatches() -1 ;
}


template<class T>
void trainDataGenerator<T>::prepareNextEpoch(){

    //prepare for next epoch, pre-read first file
    if(readthread_){
        readthread_->join(); //this is slow! FIXME: better way to exit gracefully in a simple way
        delete readthread_;

    }
    buffer_store.clear();
    buffer_read.clear();
    filecount_=0;
    nsamplesprocessed_=0;
    batchcount_=0;
    nextread_ = orig_infiles_.at(shuffle_indices_.at(filecount_));
    readthread_ = new std::thread(&trainDataGenerator<T>::readBuffer,this);
}
template<class T>
void trainDataGenerator<T>::end(){
    if(readthread_){
        readthread_->join(); //this is slow! FIXME: better way to exit gracefully in a simple way
        delete readthread_;
        readthread_=0;
    }
}


template<class T>
void trainDataGenerator<T>::clear(){
    end();
    orig_infiles_.clear();
    shuffle_indices_.clear();
    orig_rowsplits_.clear();
    splits_.clear();
    usebatch_.clear();
    randomcount_=0;

    //batchsize_ keep batch size
    //sqelementslimit_ keep
    //skiplargebatches_ keep
    buffer_store.clear();
    buffer_read.clear();

    filecount_=0;
    nbatches_=0;
    ntotal_=0;
    nsamplesprocessed_=0;
    lastbatchsize_=0;
    // filetimeout_ keep
    batchcount_=0;
}

template<class T>
trainData<T> trainDataGenerator<T>::getBatch(){
    return prepareBatch();
}

template<class T>
trainData<T>  trainDataGenerator<T>::prepareBatch(){

    size_t bufferelements=buffer_store.nElements();
    size_t expect_batchelements = splits_.at(batchcount_);
    bool usebatch = true;
    
    if(usebatch_.size())
        usebatch = usebatch_.at(batchcount_);

    if(debug)
        std::cout << "expect_batchelements "<<expect_batchelements << " vs " << bufferelements <<" bufferelements" << std::endl;

    while(bufferelements<expect_batchelements){
        //if thread, read join
        if(readthread_){
            readthread_->join();
            delete readthread_;
            readthread_=0;
        }
        buffer_store.append(buffer_read);
        buffer_read.clear();
        bufferelements = buffer_store.nElements();

        if(debug)
            std::cout << "nprocessed " << nsamplesprocessed_ << " file " << filecount_ << " in buffer " << bufferelements
            << " file read " << nextread_ << " totalfiles " << orig_infiles_.size()
            << " total events "<< ntotal_<< std::endl;

        if(nsamplesprocessed_ + bufferelements < ntotal_){
            if (filecount_ >= orig_infiles_.size())
                throw std::runtime_error(
                        "trainDataGenerator<T>::getBatch: more batches requested than data in the sample");

            nextread_ = orig_infiles_.at(shuffle_indices_.at(filecount_));

            if(debug)
                std::cout << "start new read on file "<< nextread_ <<std::endl;

            filecount_++;
            readthread_ = new std::thread(&trainDataGenerator<T>::readBuffer,this);
        }
    }

    auto thisbatch = buffer_store.split(expect_batchelements);
    if(thisbatch.nTotalElements() < 1){
      //not sure why this can happen, there might be some bigger problem here. This at least prevents crashes.
      return prepareBatch();
    }

    if(debug)
        std::cout << "providing batch " << nsamplesprocessed_ << "-" << nsamplesprocessed_+expect_batchelements <<
        " elements in buffer before: " << bufferelements <<
        "\nsplitting at " << expect_batchelements << " use this batch "<<  usebatch
        << " total elements " << thisbatch.nTotalElements() << std::endl;

    if(debug){
        int dbpcount=0;
        for(const auto& s: buffer_store.featureArray(0).rowsplits()){
            std::cout << s << ", ";
            if(dbpcount>50)break;
            dbpcount++;
        }
        std::cout << std::endl;
    }

    nsamplesprocessed_+=expect_batchelements;
    lastbatchsize_ = expect_batchelements;

    batchcount_++;
    if(! usebatch){//until valid batch
        return prepareBatch();
    }
    return thisbatch;

}



}//namespace
#endif /* DJCDEV_DEEPJETCORE_COMPILED_INTERFACE_TRAINDATAGENERATOR_H_ */
