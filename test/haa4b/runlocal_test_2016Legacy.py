import FWCore.ParameterSet.Config as cms

from UserCode.bsmhiggs_fwk.mainNtuplizer_cfi import *

#from JMEAnalysis.JetToolbox.jetToolbox_cff import jetToolbox

from PhysicsTools.PatAlgos.tools.jetTools import updateJetCollection ###
from PhysicsTools.PatUtils.tools.runMETCorrectionsAndUncertainties import runMetCorAndUncFromMiniAOD

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 1

process.load("PhysicsTools.PatAlgos.producersLayer1.patCandidates_cff")
process.load("Configuration.EventContent.EventContent_cff")
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(-1) )

#load run conditions
#process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
process.load('Configuration.Geometry.GeometryIdeal_cff')
process.load('Configuration.StandardSequences.MagneticField_38T_cff')

#------ Declare the correct global tag ------#
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_condDBv2_cff')
#process.GlobalTag.globaltag = '106X_dataRun2_v37'  #data
process.GlobalTag.globaltag = '106X_mcRun2_asymptotic_v17'

process.options   = cms.untracked.PSet(
   allowUnscheduled = cms.untracked.bool(True)
)

updateJetCollection(
   process, 
   jetSource = cms.InputTag('slimmedJets'), 
   labelName = 'UpdatedJEC', 
   jetCorrections = ('AK4PFchs', cms.vstring(['L1FastJet', 'L2Relative', 'L3Absolute', 'L2L3Residual']), 'None') 
# Update: Safe to always add 'L2L3Residual' as MC contains dummy L2L3Residual corrections (always set to 1)
) 
from RecoEgamma.EgammaTools.EgammaPostRecoTools import setupEgammaPostRecoSeq 
from RecoJets.JetProducers.PileupJetID_cfi import pileupJetId
glob_tag = '106X_mcRun2_asymptotic_v17'
if glob_tag=='106X_mcRun2_asymptotic_preVFP_v11':  #check if you are preVFP
   #re-run the Pileup Jet ID
   #https://twiki.cern.ch/twiki/bin/view/CMS/PileupJetIDUL#Recommendations_for_2016_UL_data
   from RecoJets.JetProducers.PileupJetID_cfi import _chsalgos_106X_UL16APV   #(_chsalgos_106X_UL16APV for APV samples)
   process.pileupJetIdUpdated = pileupJetId.clone( 
           jets=cms.InputTag('updatedPatJetsUpdatedJEC'),		       #(Your JEC corrected jets here),
           inputIsCorrected=True,
           applyJec=False,
           vertexes=cms.InputTag("offlineSlimmedPrimaryVertices"),
           algos = cms.VPSet(_chsalgos_106X_UL16APV),
       )

   #Recipe for running scales and smearings using EgammaPostRecoTools
   #https://twiki.cern.ch/twiki/bin/view/CMS/EgammaUL2016To2018#Scale_and_smearing_corrections_f
   setupEgammaPostRecoSeq(process,
                          runEnergyCorrections=True,
                          runVID=False, #saves CPU time by not needlessly re-running VID, if you want the Fall17V2 IDs, set this to True or remove (default is True)
                          era='2016preVFP-UL')    
   #a sequence egammaPostRecoSeq has now been created and should be added to your path, eg process.p=cms.Path(process.egammaPostRecoSeq)
else:
   #re-run the Pileup ID
   #https://twiki.cern.ch/twiki/bin/view/CMS/PileupJetIDUL#Recommendations_for_2016_UL_data
   from RecoJets.JetProducers.PileupJetID_cfi import _chsalgos_106X_UL16     # (or _chsalgos_106X_UL16APV for APV samples)
   process.pileupJetIdUpdated = pileupJetId.clone( 
	   jets=cms.InputTag('updatedPatJetsUpdatedJEC'),		     # (Your JEC corrected jets here),
           inputIsCorrected=True,
           applyJec=False,
           vertexes=cms.InputTag("offlineSlimmedPrimaryVertices"),
           algos = cms.VPSet(_chsalgos_106X_UL16),
       )
   #Recipe for running scales and smearings using EgammaPostRecoTools
   #https://twiki.cern.ch/twiki/bin/view/CMS/EgammaUL2016To2018#Scale_and_smearing_corrections_f
   setupEgammaPostRecoSeq(process,
                          runEnergyCorrections=True,
                          runVID=False, #saves CPU time.. (see above)
                          era='2016postVFP-UL')    
   #Note: To change the era to:
   # 2016ULpreVFP : era='2016preVFP-UL'
   # 2016ULpostVFP : era='2016postVFP-UL'
   # 2017 UL, era='2017-UL'
   # 2018 UL, era='2018-UL' 

runOnMC=True  
if runOnMC:
   runMetCorAndUncFromMiniAOD(process,  
   			isData=False
			)		
else:
   runMetCorAndUncFromMiniAOD(process,
                        isData=True,
                        )

process.mainNtuplizer.isMC = cms.bool(True)
#process.mainNtuplizer.isMC = cms.bool(False)
process.mainNtuplizer.dtag = cms.string("MC13TeV_Zh_amass15_2016")
#process.mainNtuplizer.dtag = cms.string("Data13TeV_MuEG2016B_ver2")
process.mainNtuplizer.xsec = cms.double(1.0)
process.mainNtuplizer.mctruthmode = cms.int32(1)
process.mainNtuplizer.verbose = cms.bool(False)
process.mainNtuplizer.metFilterBitsTag = cms.InputTag('TriggerResults','','HLT')
process.mainNtuplizer.Legacy2016 = cms.bool(True)
process.mainNtuplizer.jetsTag = cms.InputTag('updatedPatJetsUpdatedJEC')  
#process.mainNtuplizer.jetsTag = cms.InputTag('selectedUpdatedPatJets')
#process.mainNtuplizer.fatjetsTag = cms.InputTag('slimmedJetsAK8PFPuppiSoftDropPacked')

process.source = cms.Source("PoolSource",
	       	     fileNames = cms.untracked.vstring("/store/mc/RunIISummer20UL16MiniAODv2/SUSY_ZH_ZToAll_HToAATo4B_M-15_TuneCP5_13TeV_madgraph_pythia8/MINIAODSIM/106X_mcRun2_asymptotic_v17-v2/270000/A41D8B43-BB83-6D43-9061-CC066509DB62.root"),
		     inputCommands=cms.untracked.vstring(
			   'keep *', 
 		     	   'drop *_ctppsLocalTrackLiteProducer_*_RECO' 
			   )        
)

#jetToolbox(process, 'ak8', 'jetSequence', 'out', PUMethod='CHS', addPruning=True, addSoftDrop=True,addNsubSubjets=True,addSoftDropSubjets=True,addNsub=True,Cut="pt>20")

process.TFileService = cms.Service("TFileService",
			fileName = cms.string("analysis_SUSYZHZToAllToAATo4B_M15.root")
)

from PhysicsTools.PatAlgos.tools.helpers  import getPatAlgosToolsTask
process.patAlgosToolsTask = getPatAlgosToolsTask(process)
process.pathRunPatAlgos = cms.Path(process.patAlgosToolsTask)              		 				             
process.p = cms.Path( 
		process.patJetCorrFactorsUpdatedJEC * 
		process.updatedPatJetsUpdatedJEC * 
 		process.pileupJetIdUpdated *
		process.fullPatMetSequence * 
		process.egammaPostRecoSeq *
		process.mainNtuplizer )	

