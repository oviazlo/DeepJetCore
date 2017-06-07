#infile='<tree_association.txt file generated by the predict.py script>'

infile='tree_association.txt'
from testing import makePlots_async

makePlots_async(infile,      #input file or file list
                ['test'],    #legend names (needs to be list)
                'jet_pt',    #variable to plot
                'jet_pt>40', #cut to apply
                'green',     #line color and style (e.g. 'red,dashed')
                'test.pdf',  #output file (pdf)
                'xaxis',     #xaxisname
                'yaxis',     #yaxisname
                False)       #normalize



makePlots_async(infile,      #input file or file list
                ['central','forward'],    #legend names (needs to be list)
                'jet_pt',    #variable to plot
                ['jet_eta<1.1 && jet_eta>-1.1','jet_eta<-1.1 || jet_eta>1.1'], #cut to apply
                ['green','red'],     #line color and style (e.g. 'red,dashed')
                'test2.pdf',  #output file (pdf)
                'xaxis',     #xaxisname
                'yaxis',     #yaxisname
                True)       #normalise

makePlots_async(infile,['B','C','UDS'],'(reg_Pt-gen_pt_WithNu)/gen_pt_WithNu',["isB+isBB+isLeptonicB+isLeptonicB_C &&  gen_pt_WithNu>30","isC &&  gen_pt_WithNu>30","isUD+isS+isG &&  gen_pt_WithNu>30"],['green','red','blue'],'resol.pdf','resol','jets',True)


makePlots_async(infile,['B','C','UDS'],'(reg_Pt-gen_pt_WithNu)/reg_uncPt',["isB+isBB+isLeptonicB+isLeptonicB_C &&  gen_pt_WithNu>30","isC &&  gen_pt_WithNu>30","isUD+isS+isG &&  gen_pt_WithNu>30"],['green','red','blue'],'pull.pdf','pull','jets',True)

makePlots_async(infile,['inclusive'],'(reg_Pt-gen_pt_WithNu)/gen_pt_WithNu',["gen_pt_WithNu>30"],['green'],'resol_incl.pdf','resol','jets',True)

makePlots_async(infile,['inclusive'],'(reg_Pt-gen_pt_WithNu)/reg_uncPt',["gen_pt_WithNu>30"],['green'],'pull_incl.pdf','pull','jets',True)

makePlots_async(infile,['inclusive'],'(reg_Pt-gen_pt_WithNu)/gen_pt_WithNu:gen_pt_WithNu',["gen_pt_WithNu>30"],['green'],'resol_v_gen.pdf','gen Pt','resol',False)

#makePlots_async(infile,['inclusive'],'(reg_Pt-gen_pt_WithNu)/reg_uncPt:gen_pt_WithNu',["gen_pt_WithNu>30"],['COLZ'],'pull_v_gen.pdf','gen Pt','pull',False)

#makePlots_async(infile,['inclusive'],'(jet_corr_pt-gen_pt_WithNu)/gen_pt_WithNu:gen_pt_WithNu',["gen_pt_WithNu>30"],['COLZ'],'resolcorPt_v_gen.pdf','gen Pt','resol (corPt)',False)


#profile plots
makePlots_async(infile, #input file or file list
                ['inclusive','B','C','UDS'], #legend names [as list]
                'reg_Pt/gen_pt_WithNu:gen_pt_WithNu', #variable to plot --> yaxis:xaxis
                ["gen_pt_WithNu>30&&gen_pt_WithNu<300","isB+isBB+isLeptonicB+isLeptonicB_C &&  gen_pt_WithNu>30&&gen_pt_WithNu<300","isC &&  gen_pt_WithNu>30&&gen_pt_WithNu<300","isUD+isS+isG &&  gen_pt_WithNu>30&&gen_pt_WithNu<300"], #list of cuts to apply
                ['black','green','red','blue'], #list of color and style, e.g. ['red,dashed', ...]
                'resol_v_gen_Profile.pdf', #output file (pdf)
                'gen Pt', #xaxisname
                'response', #yaxisname
                False, #normalize
                True, #make a profile plot
                0.7, #override min value of y-axis range
                1.3) #override max value of y-axis range

makePlots_async(infile,['inclusive','B','C','UDS'],'jet_corr_pt/gen_pt_WithNu:gen_pt_WithNu',["gen_pt_WithNu>30&&gen_pt_WithNu<300","isB+isBB+isLeptonicB+isLeptonicB_C &&  gen_pt_WithNu>30&&gen_pt_WithNu<300","isC &&  gen_pt_WithNu>30&&gen_pt_WithNu<300","isUD+isS+isG &&  gen_pt_WithNu>30&&gen_pt_WithNu<300"],['black','green','red','blue'],'resol_v_gen_Profile_Default.pdf','gen Pt','response',False,True,0.7,1.3)


