//#define YEAR_2017

#include <iostream>
#include <map>

#include "FWCore/FWLite/interface/FWLiteEnabler.h"
#include "FWCore/PythonParameterSet/interface/MakeParameterSets.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "UserCode/bsmhiggs_fwk/interface/PatUtils.h"
#include "UserCode/bsmhiggs_fwk/interface/MacroUtils.h"
#include "UserCode/bsmhiggs_fwk/interface/DataEvtSummaryHandler.h"
#include "UserCode/bsmhiggs_fwk/interface/MVAHandler.h"
#include "UserCode/bsmhiggs_fwk/interface/TMVAReader.h"
#include "UserCode/bsmhiggs_fwk/interface/BSMPhysicsEvent.h"
#include "UserCode/bsmhiggs_fwk/interface/SmartSelectionMonitor.h"
#include "UserCode/bsmhiggs_fwk/interface/PDFInfo.h"
//#include "UserCode/bsmhiggs_fwk/interface/rochcor2016.h" 
#include "UserCode/bsmhiggs_fwk/interface/RoccoR.h" 
//#include "UserCode/bsmhiggs_fwk/interface/muresolution_run2.h" 
#include "UserCode/bsmhiggs_fwk/interface/LeptonEfficiencySF.h"
//#include "UserCode/bsmhiggs_fwk/interface/BTagCalibrationStandalone.h"
#include "UserCode/bsmhiggs_fwk/interface/BtagUncertaintyComputer.h"
#include "UserCode/bsmhiggs_fwk/interface/METUtils.h"
//#include "UserCode/bsmhiggs_fwk/interface/BTagUtils.h"
#include "UserCode/bsmhiggs_fwk/interface/EventCategory.h"
#include "UserCode/bsmhiggs_fwk/interface/statWgt.h"

#include "EgammaAnalysis/ElectronTools/interface/EnergyScaleCorrection_class.h"
#include "CondFormats/JetMETObjects/interface/JetResolution.h"
#include "CondFormats/JetMETObjects/interface/JetCorrectionUncertainty.h"
//#include "PhysicsTools/Utilities/interface/LumiReWeighting.h"

//https://twiki.cern.ch/twiki/bin/view/CMS/BTagCalibration
#include "CondFormats/BTauObjects/interface/BTagCalibration.h"
#include "CondTools/BTau/interface/BTagCalibrationReader.h"

#include "TSystem.h"
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TEventList.h"
#include "TROOT.h"
#include "TMath.h"

#include <unistd.h>
using namespace std;

// histograms and functions for btagging SFs
TH1D* h_csv_wgt_hf[9][5];
TH1D* c_csv_wgt_hf[9][5];
TH1D* h_csv_wgt_lf[9][4][3];
void fillCSVhistos(TFile *fileHF, TFile *fileLF);
double get_csv_wgt(double jetPt, double jetEta, double jetCSV, int jetFlavor, int iSys, double &csvWgtHF, double &csvWgtLF, double &scvWgtCF);

namespace LHAPDF {
void initPDFSet(int nset, const std::string& filename, int member=0);
int numberPDF(int nset);
void usePDFMember(int nset, int member);
double xfx(int nset, double x, double Q, int fl);
double getXmin(int nset, int member);
double getXmax(int nset, int member);
double getQ2min(int nset, int member);
double getQ2max(int nset, int member);
void extrapolate(bool extrapolate=true);
}

struct stPDFval {
    stPDFval() {}
    stPDFval(const stPDFval& arg) :
        qscale(arg.qscale),
        x1(arg.x1),
        x2(arg.x2),
        id1(arg.id1),
        id2(arg.id2) {
    }

    double qscale;
    double x1;
    double x2;
    int id1;
    int id2;
};

struct ptsort: public std::binary_function<LorentzVector, LorentzVector, bool> 
{
  bool operator () (const LorentzVector & x, const LorentzVector & y) 
  { return  ( x.pt() > y.pt() ) ; }
};
struct btagsort: public std::binary_function<PhysicsObject_Jet, PhysicsObject_Jet, float> 
{
  bool operator () (const PhysicsObject_Jet & x, PhysicsObject_Jet & y) 
  { return  ( x.btag0 > y.btag0 ) ; }
}; 

//bool runDBversion = false;

// Physics objects offline thresholds, default values for 2016 below. 2017 value is updated in code
//const float lep_threshold_=25.; 
float mu_threshold_=10.; //25.; 
float ele_threshold_=15.; //30.; 
const float jet_threshold_=20.; 

//https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagRecommendation76X
float CSVLooseWP = 0.5426;  // Updated to 80X Moriond17 Loose
float CSVMediumWP = 0.800;
float CSVTightWP = 0.935;

//https://twiki.cern.ch/twiki/bin/viewauth/CMS/Hbbtagging#8_0_X
// https://indico.cern.ch/event/543002/contributions/2205058/attachments/1295600/1932387/cv-doubleb-tagging-btv-approval.pdf (definition of WPs: slide 16)
const float DBLooseWP = 0.300;
const float DBMediumWP = 0.600;
const float DBTightWP = 0.900;

//https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagRecommendation80XReReco#Data_MC_Scale_Factors
float DeepCSVLooseWP = 0.2219;
float DeepCSVMediumWP = 0.6324;
float DeepCSVTightWP = 0.8958;

int main(int argc, char* argv[])
{
    //##################################################################################
    //##########################    GLOBAL INITIALIZATION     ##########################
    //##################################################################################

    //check arguments
    if(argc<2) {
        std::cout << "Usage : " << argv[0] << " parameters_cfg.py" << std::endl;
        exit(0);
    }
   
    //load framework libraries
    gSystem->Load( "libFWCoreFWLite" );
    //AutoLibraryLoader::enable();
    FWLiteEnabler::enable();

    //configure the process
    const edm::ParameterSet &runProcess = edm::readPSetsFrom(argv[1])->getParameter<edm::ParameterSet>("runProcess");

    bool runZH = runProcess.getParameter<bool>("runZH");
  
    bool isMC = runProcess.getParameter<bool>("isMC");
    double xsec = runProcess.getParameter<double>("xsec");

    TString proc=runProcess.getParameter<std::string>("proc");
    TString dtag=runProcess.getParameter<std::string>("tag");
    TString suffix=runProcess.getParameter<std::string>("suffix");

    TString btagDir=runProcess.getParameter<std::string>("btagDir");
    TString zptDir=runProcess.getParameter<std::string>("zptDir");

    bool is2016data = (!isMC && dtag.Contains("2016"));
    bool is2016MC = (isMC && dtag.Contains("2016"));
    bool is2016Signal = (is2016MC && dtag.Contains("h_amass"));
    bool is2017data = (!isMC && dtag.Contains("2017"));
    bool is2017MC = (isMC && dtag.Contains("2017"));
    bool is2018data = (!isMC && dtag.Contains("2018"));
    bool is2018MC = (isMC && dtag.Contains("2018"));
    bool is2017BCdata = (is2017data && (dtag.Contains("2017B") || dtag.Contains("2017C")));
    bool afterRun319077 = false;
    bool jetinHEM = false;
    bool eleinHEM = false;
    bool is2017_2018 = (is2017MC || is2017data || is2018MC || is2018data);

    bool verbose = runProcess.getParameter<bool>("verbose");
   
    int mctruthmode = runProcess.getParameter<int>("mctruthmode");

    // will reweight the top pt in TT+jets sample (optional)
    bool reweightTopPt = runProcess.getParameter<bool>("reweightTopPt");

    // will reweight the Z pt in DY+jets sample (optional)
    bool reweightDYZPt = runProcess.getParameter<bool>("reweightDYZPt");

    // will produce the input root trees to BDT training (optional)
    bool runMVA = runProcess.getParameter<bool>("runMVA");

    // Will set DeepCSV as the default b-tagger automaticaly:
    bool use_DeepCSV = runProcess.getParameter<bool>("useDeepCSV");
    bool runQCD = runProcess.getParameter<bool>("runQCD");
    
    bool usemetNoHF = runProcess.getParameter<bool>("usemetNoHF");
    
    // choose which method to use to apply btagging efficiency scale factors
    // nMethod=1 is Jet-by-jet updating of b-tagging status https://twiki.cern.ch/twiki/bin/view/CMS/BTagSFMethods#2a_Jet_by_jet_updating_of_the_b
    // nMethod=2 is event reweighting https://twiki.cern.ch/twiki/bin/view/CMS/BTagSFMethods#1d_Event_reweighting_using_discr
    int nMethod = runProcess.getParameter<int>("btagSFMethod");

    TString url = runProcess.getParameter<std::string>("input");
    TString outFileUrl( dtag ); //gSystem->BaseName(url));

    if(!use_DeepCSV && (is2018data || is2018MC)){
      std::cout << "2018 Data does not have CSV, use DeepCSV instead!" << std::endl;
      exit(0);
    }

    if(is2017data || is2017MC){
      // 2017 Btag Recommendation: https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagRecommendation94X
        CSVLooseWP = 0.5803; CSVMediumWP = 0.8838; CSVTightWP = 0.9693;
        DeepCSVLooseWP = 0.1522; DeepCSVMediumWP = 0.4941; DeepCSVTightWP = 0.8001;
	//        mu_threshold_=30.;
	//        ele_threshold_=35.;
    }

    if(is2018data || is2018MC){ // https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagRecommendation102X
      DeepCSVLooseWP = 0.1241; DeepCSVMediumWP = 0.4184; DeepCSVTightWP = 0.7527;
  //    ele_threshold_=35.; mu_threshold_=25.;
    }

    if(is2016Signal){//https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagRecommendation2016Legacy
      DeepCSVLooseWP = 0.2217; DeepCSVMediumWP = 0.6321; DeepCSVTightWP = 0.8953;
    }

    // if(mctruthmode!=0) {
    //     outFileUrl += "_filt";
    //     outFileUrl += mctruthmode;
    // }
    TString outdir = runProcess.getParameter<std::string>("outdir");
    TString outUrl( outdir );
    TString outTxtUrl = outUrl + "/TXT";
    gSystem->Exec("mkdir -p " + outUrl);
    gSystem->Exec("mkdir -p " + outTxtUrl);
  
    TString outTxtUrl_final= outTxtUrl + "/" + outFileUrl + "_FinalList.txt";
    FILE* outTxtFile_final = NULL;
    outTxtFile_final = fopen(outTxtUrl_final.Data(), "w");
    printf("TextFile URL = %s\n",outTxtUrl_final.Data());
    fprintf(outTxtFile_final,"run lumi event\n");

    int fType(0);
    if(dtag.Contains("DoubleEle")) fType=EE;
    if(dtag.Contains("DoubleMuon"))  fType=MUMU;
    if(dtag.Contains("MuEG"))    fType=EMU;
    if(dtag.Contains("SingleMuon")) {
      //      if (runZH) fType=MUMU;
      fType=MU;
    }
    if(dtag.Contains("SingleElectron")) {
      //      if(runZH) fType=EE;
      fType=E;
    }

    bool isSingleMuPD(!isMC && dtag.Contains("SingleMuon"));
    bool isDoubleMuPD(!isMC && dtag.Contains("DoubleMuon"));
    bool isSingleElePD(!isMC && dtag.Contains("SingleElectron"));
    bool isDoubleElePD(!isMC && dtag.Contains("DoubleEle"));
    bool isMuonEGPD(!isMC && dtag.Contains("MuEG"));

    if(is2018data && dtag.Contains("EGamma")) {
      if(runZH){ 
	fType=EE; isDoubleElePD=true;
      } else {
	fType=E; isSingleElePD=true;
      }
    }
    
    bool isMC_ZZ  = isMC && ( string(url.Data()).find("TeV_ZZ_")  != string::npos );
    
    bool isMC_WZ  = isMC && ( string(url.Data()).find("TeV_WZamcatnloFXFX")  != string::npos
                              || string(url.Data()).find("MC13TeV_WZpowheg")  != string::npos );

    bool isMC_VVV = isMC && ( string(url.Data()).find("MC13TeV_WZZ")  != string::npos
                              || string(url.Data()).find("MC13TeV_WWZ")  != string::npos
                              || string(url.Data()).find("MC13TeV_ZZZ")  != string::npos );

    bool isMCBkg_runPDFQCDscale = (isMC_ZZ || isMC_WZ || isMC_VVV);

    bool isMC_ttbar = isMC && (string(url.Data()).find("TeV_TTJets")  != string::npos);
    if(is2017data || is2017MC || is2018data || is2018MC) isMC_ttbar = isMC && (string(url.Data()).find("TeV_TTTo")  != string::npos);
    bool isMC_stop  = isMC && (string(url.Data()).find("TeV_SingleT")  != string::npos);

    bool isMC_WJets = isMC && ( (string(url.Data()).find("MC13TeV_WJets")  != string::npos) || (string(url.Data()).find("MC13TeV_W1Jets")  != string::npos) || (string(url.Data()).find("MC13TeV_W2Jets")  != string::npos) || (string(url.Data()).find("MC13TeV_W3Jets")  != string::npos) || (string(url.Data()).find("MC13TeV_W4Jets")  != string::npos) );
    bool isMC_DY = isMC && ( (string(url.Data()).find("MC13TeV_DY")  != string::npos) );
    
    bool isMC_Wh = isMC && (string(url.Data()).find("Wh")  != string::npos); 
    bool isMC_Zh = isMC && (string(url.Data()).find("Zh")  != string::npos); 
    bool isMC_VBF = isMC && (string(url.Data()).find("VBF")  != string::npos); 

    bool isQCD = isMC && (string(url.Data()).find("QCD")  != string::npos);
    bool isMC_QCD_MuEnr = isMC && (string(url.Data()).find("MuEnr")  != string::npos);
    bool isMC_QCD_EMEnr = isMC && (string(url.Data()).find("EMEnr")  != string::npos);
    
    bool isSignal = (isMC_Wh || isMC_Zh || isMC_VBF );

    if (isSignal) printf("Signal url = %s\n",url.Data());

    if (!runZH) {
      if(isDoubleElePD || isDoubleMuPD || isMuonEGPD) return -1; 
    }   

    //b-tagging: beff and leff must be derived from the MC sample using the discriminator vs flavor
    //the scale factors are taken as average numbers from the pT dependent curves see:
    //https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagPOG#2012_Data_and_MC_EPS13_prescript
    BTagSFUtil btsfutil;
    float beffLoose(0.68), beffMedium(0.68), sfb(0.99), sfbunc(0.015);
    float leffLoose(0.13), leffMedium(0.13), sflMedium(1.05), sflunc(0.12);

    //read btagging efficiency SFs from root files
    //https://twiki.cern.ch/twiki/bin/view/CMS/BTagShapeCalibration
    std::string inputFileHF, inputFileLF;
    inputFileHF = "sfs_deepcsv_2016_hf.root";
    inputFileLF = "sfs_deepcsv_2016_lf.root";
    if(is2017data || is2017MC){
      inputFileHF = "sfs_deepcsv_2017_hf.root";
      inputFileLF = "sfs_deepcsv_2017_lf.root";
    } else if (is2018data || is2018MC){
      inputFileHF = "sfs_deepcsv_2018_hf.root";
      inputFileLF = "sfs_deepcsv_2018_lf.root";
    }

    //setup calibration readers 80X
    std::string b_tagging_name, csv_file_path, csv_file_path1, csv_file_path2, csv_file_path3;
    float  bTag_lumi1 = 4.823, bTag_lumi2 = 21.719, bTag_lumi3 = 15.017; //2017
    float LooseWP = -1, MediumWP = -1, TightWP = -1;
    if (!use_DeepCSV) 
    {
      b_tagging_name = "CSVv2";
       
      csv_file_path = std::string(std::getenv("CMSSW_BASE"))+
                      "/src/UserCode/bsmhiggs_fwk/data/weights/CSVv2_Moriond17_B_H.csv";
      if(is2017data || is2017MC){
          csv_file_path = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/CSVv2_94XSF_V2_B_F.csv";     
          csv_file_path1 = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/CSVv2_94XSF_V2_B.csv"; 
          csv_file_path2 = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/CSVv2_94XSF_V2_C_E.csv";       
          csv_file_path3 = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/CSVv2_94XSF_V2_E_F.csv";       
      }

       
      LooseWP = CSVLooseWP;
      MediumWP = CSVMediumWP;
      TightWP = CSVTightWP;
    }
    if ( use_DeepCSV) {
      b_tagging_name = "DeepCSV";
       
      csv_file_path = std::string(std::getenv("CMSSW_BASE"))+
                      "/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_Moriond17_B_H.csv";
      if(is2017data || is2017MC){
          csv_file_path = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_94XSF_V4_B_F.csv";       
          csv_file_path1 = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_94XSF_V3_B.csv";       
          csv_file_path2 = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_94XSF_V3_C_E.csv";       
          csv_file_path3 = std::string(std::getenv("CMSSW_BASE"))+
                          "/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_94XSF_V3_E_F.csv";       
      }
      if(is2018data || is2018MC){
        csv_file_path = std::string(std::getenv("CMSSW_BASE"))+
                        "/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_102XSF_V1.csv";
      }
      if(is2016Signal){
	csv_file_path = std::string(std::getenv("CMSSW_BASE"))+
			"/src/UserCode/bsmhiggs_fwk/data/weights/DeepCSV_2016LegacySF_V1.csv";
      }
      LooseWP = DeepCSVLooseWP;
      MediumWP = DeepCSVMediumWP;
      TightWP = DeepCSVTightWP;
    }
    
    BTagCalibration btagCalib(b_tagging_name, csv_file_path);
    BTagCalibration btagCalib1, btagCalib2, btagCalib3;
    // setup calibration readers 80X
    //BTagCalibrationReader80X btagCal80X(BTagEntry::OP_LOOSE, "central", {"up", "down"});
    //btagCal80X.load(btagCalib, BTagEntry::FLAV_B, "comb");
    //btagCal80X.load(btagCalib, BTagEntry::FLAV_C, "comb");
    //btagCal80X.load(btagCalib, BTagEntry::FLAV_UDSG, "incl");

    BTagCalibrationReader btagReaderLoose(BTagEntry::OP_LOOSE, "central", {"up", "down"});
    btagReaderLoose.load(btagCalib, BTagEntry::FLAV_B, "comb");
    btagReaderLoose.load(btagCalib, BTagEntry::FLAV_C, "comb");
    btagReaderLoose.load(btagCalib, BTagEntry::FLAV_UDSG, "incl");
    
    BTagCalibrationReader btagReaderMedium(BTagEntry::OP_MEDIUM, "central", {"up", "down"});
    btagReaderMedium.load(btagCalib, BTagEntry::FLAV_B, "comb");
    btagReaderMedium.load(btagCalib, BTagEntry::FLAV_C, "comb");
    btagReaderMedium.load(btagCalib, BTagEntry::FLAV_UDSG, "incl");

    BTagCalibrationReader btagReaderLoose1, btagReaderLoose2, btagReaderLoose3, btagReaderMedium1, btagReaderMedium2, btagReaderMedium3;
    if(is2017data || is2017MC){// b-tag SFs with period dependency for 2017
      btagCalib1 = BTagCalibration(b_tagging_name+"1", csv_file_path1);
      btagCalib2 = BTagCalibration(b_tagging_name+"2", csv_file_path2);
      btagCalib3 = BTagCalibration(b_tagging_name+"3", csv_file_path3);

      btagReaderLoose1 = BTagCalibrationReader(BTagEntry::OP_LOOSE, "central", {"up", "down"}); btagReaderLoose2 = BTagCalibrationReader(BTagEntry::OP_LOOSE, "central", {"up", "down"}); btagReaderLoose3 = BTagCalibrationReader(BTagEntry::OP_LOOSE, "central", {"up", "down"});
      btagReaderMedium1 = BTagCalibrationReader(BTagEntry::OP_MEDIUM, "central", {"up", "down"}); btagReaderMedium2 = BTagCalibrationReader(BTagEntry::OP_MEDIUM, "central", {"up", "down"}); btagReaderMedium3 = BTagCalibrationReader(BTagEntry::OP_MEDIUM, "central", {"up", "down"});
      btagReaderLoose1.load(btagCalib1, BTagEntry::FLAV_B, "comb"); btagReaderLoose1.load(btagCalib1, BTagEntry::FLAV_C, "comb"); btagReaderLoose1.load(btagCalib1, BTagEntry::FLAV_UDSG, "incl");
      btagReaderLoose2.load(btagCalib2, BTagEntry::FLAV_B, "comb"); btagReaderLoose2.load(btagCalib2, BTagEntry::FLAV_C, "comb"); btagReaderLoose2.load(btagCalib2, BTagEntry::FLAV_UDSG, "incl");
      btagReaderLoose3.load(btagCalib3, BTagEntry::FLAV_B, "comb"); btagReaderLoose3.load(btagCalib3, BTagEntry::FLAV_C, "comb"); btagReaderLoose3.load(btagCalib3, BTagEntry::FLAV_UDSG, "incl");
      btagReaderMedium1.load(btagCalib1, BTagEntry::FLAV_B, "comb"); btagReaderMedium1.load(btagCalib1, BTagEntry::FLAV_C, "comb"); btagReaderMedium1.load(btagCalib1, BTagEntry::FLAV_UDSG, "incl");
      btagReaderMedium2.load(btagCalib2, BTagEntry::FLAV_B, "comb"); btagReaderMedium2.load(btagCalib2, BTagEntry::FLAV_C, "comb"); btagReaderMedium2.load(btagCalib2, BTagEntry::FLAV_UDSG, "incl");
      btagReaderMedium3.load(btagCalib3, BTagEntry::FLAV_B, "comb"); btagReaderMedium3.load(btagCalib3, BTagEntry::FLAV_C, "comb"); btagReaderMedium3.load(btagCalib3, BTagEntry::FLAV_UDSG, "incl");

    }

    //jet energy scale uncertainties
    TString jecDir = runProcess.getParameter<std::string>("jecDir");
    //gSystem->ExpandPathName(uncFile);
    cout << "Loading jet energy scale uncertainties from: " << jecDir << endl;

    if(is2018MC || is2018data){
        if     (dtag.Contains("2018A")) jecDir+="102X/Autumn18_V8_RunA/Autumn18_RunA_V8_";
        else if(dtag.Contains("2018B")) jecDir+="102X/Autumn18_V8_RunB/Autumn18_RunB_V8_";
        else if(dtag.Contains("2018C")) jecDir+="102X/Autumn18_V8_RunC/Autumn18_RunC_V8_";
        else if(dtag.Contains("2018D")) jecDir+="102X/Autumn18_V8_RunD/Autumn18_RunD_V8_";
        if(isMC) {jecDir+="102X/Autumn18_V8_MC/Autumn18_V8_";}
    }
    else if(is2017MC || is2017data){
        if     (dtag.Contains("2017B")) jecDir+="94X/Fall17_17Nov2017B_V32_DATA/Fall17_17Nov2017B_V32_";
        else if(dtag.Contains("2017C")) jecDir+="94X/Fall17_17Nov2017C_V32_DATA/Fall17_17Nov2017C_V32_";
        else if(dtag.Contains("2017D") || dtag.Contains("2017E")) jecDir+="94X/Fall17_17Nov2017DE_V32_DATA/Fall17_17Nov2017DE_V32_";
        else if(dtag.Contains("2017F")) jecDir+="94X/Fall17_17Nov2017F_V32_DATA/Fall17_17Nov2017F_V32_";
        if(isMC) {jecDir+="94X/Fall17_17Nov2017_V32_MC/Fall17_17Nov2017_V32_";}
    }
    else{//2016 jec files
        if     (dtag.Contains("2016B") || dtag.Contains("2016C") ||dtag.Contains("2016D")) jecDir+="Summer16_80X/Summer16_23Sep2016BCDV4_DATA/";
        else if(dtag.Contains("2016E") || dtag.Contains("2016F")) jecDir+="Summer16_80X/Summer16_23Sep2016EFV4_DATA/";
        else if(dtag.Contains("2016G")) jecDir+="Summer16_80X/Summer16_23Sep2016GV4_DATA/";
        else if(dtag.Contains("2016H")) jecDir+="Summer16_80X/Summer16_23Sep2016HV4_DATA/";
	if(is2016Signal) {jecDir+="Summer16_94X/Summer16_07Aug2017_V11_MC/Summer16_07Aug2017_V11_";}
        else if(isMC) {jecDir+="Summer16_80X/Summer16_23Sep2016V4_MC/";}
    }


    gSystem->ExpandPathName(jecDir);
    //    FactorizedJetCorrector *jesCor = NULL;
    //    jesCor = utils::cmssw::getJetCorrector(jecDir,isMC);

    TString pf(isMC ? "MC" : "DATA");

    // Instantiate uncertainty sources
    const int nsrc = 6; //27;
    const char* srcnames[nsrc] =
      {"SubTotalPileUp", "SubTotalRelative", "SubTotalPt", "SubTotalScale", "SubTotalAbsolute","SubTotalMC"};
    /*
      {"AbsoluteStat", "AbsoluteScale", "AbsoluteFlavMap", "AbsoluteMPFBias", "Fragmentation",
       "SinglePionECAL", "SinglePionHCAL",
       "FlavorQCD", "TimePtEta",
       "RelativeJEREC1", "RelativeJEREC2", "RelativeJERHF",
       "RelativePtBB","RelativePtEC1", "RelativePtEC2", "RelativePtHF","RelativeBal", "RelativeFSR",
       "RelativeStatFSR", "RelativeStatEC", "RelativeStatHF",
       "PileUpDataMC", "PileUpPtRef", "PileUpPtBB", "PileUpPtEC1", "PileUpPtEC2", "PileUpPtHF"
      };
    */
    std::vector<JetCorrectionUncertainty*> totalJESUnc(nsrc);// = NULL; //(nsrc);
    
    for (int isrc = 0; isrc < nsrc; isrc++) {
      const char *name = srcnames[isrc];
      JetCorrectorParameters *p = new JetCorrectorParameters((jecDir+pf+"_UncertaintySources_AK4PFchs.txt").Data(), name);
      JetCorrectionUncertainty *unc = new JetCorrectionUncertainty(*p);
      //      totalJESUnc->push_back(unc);
      totalJESUnc[isrc] = unc;
    }

    
    //systematics
    bool runSystematics = runProcess.getParameter<bool>("runSystematics");
    std::vector<TString> varNames(1,"");

    std::vector<string> eleVarNames = {""};
    //    std::vector<string> jetVarNames = {"", "_scale_jup","_scale_jdown", "_res_jup", "_res_jdown","_btagup","_btagdown","_bnorm_up","_bnorm_down"};

    if(runSystematics) {
      cout << "Systematics will be computed for this analysis: " << endl;
      
      eleVarNames.push_back("_stat_eup");
      eleVarNames.push_back("_stat_edown");
      eleVarNames.push_back("_sys_eup");
      eleVarNames.push_back("_sys_edown");
      // eleVarNames.push_back("_GS_eup");
      // eleVarNames.push_back("_GS_edown");
      eleVarNames.push_back("_resRho_eup");
      eleVarNames.push_back("_resRho_edown");
      eleVarNames.push_back("_resPhi_edown");
      
      varNames.push_back("_jerup"); 	//1 
      varNames.push_back("_jerdown"); //2
      
      for (int isrc = 0; isrc < nsrc; isrc++) {
	const char *name = srcnames[isrc];
	
	stringstream ss;string target;
	ss << name; ss >> target;
	varNames.push_back("_"+target+"_jesup");      
	varNames.push_back("_"+target+"_jesdown");
      }
      
      varNames.push_back("_umetup");        //5
      varNames.push_back("_umetdown");//6 
      varNames.push_back("_lesup");         //7 
      varNames.push_back("_lesdown"); //8  
      
      varNames.push_back("_btagup"); varNames.push_back("_btagdown");//11, 12
      varNames.push_back("_ctagup"); varNames.push_back("_ctagdown");//13, 14
      varNames.push_back("_ltagup"); varNames.push_back("_ltagdown");//15, 16
      
      varNames.push_back("_softbup"); varNames.push_back("_softbdown");//11, 12      
      //	  varNames.push_back("_bnormup"); varNames.push_back("_bnormdown");//11, 12   
      //
      //varNames.push_back("_scale_mup");    varNames.push_back("_scale_mdown");  //muon energy scale
      varNames.push_back("_stat_eup");    varNames.push_back("_stat_edown");  //electron energy scale
      varNames.push_back("_sys_eup");    varNames.push_back("_sys_edown");  //electron energy scale
      //  varNames.push_back("_GS_eup");    varNames.push_back("_GS_edown");  //electron energy scale
      varNames.push_back("_resRho_eup");    varNames.push_back("_resRho_edown");  //electron energy resolution
      varNames.push_back("_resPhi_edown");     //electron energy resolution
      
      varNames.push_back("_puup");  varNames.push_back("_pudown");      //pileup uncertainty 
      varNames.push_back("_pdfup"); varNames.push_back("_pdfdown");  
      
      //	  varNames.push_back("_th_pdf");                                           //pdf
	  //	  varNames.push_back("_th_alphas"); //alpha_s (QCD)

	  /*
	  if(isMCBkg_runPDFQCDscale) {
            varNames.push_back("_pdfup");
            varNames.push_back("_pdfdown");
            varNames.push_back("_qcdscaleup");
            varNames.push_back("_qcdscaledown");
	    
	  }
	  if(isSignal) {
            varNames.push_back("_pdfup");
            varNames.push_back("_pdfdown");
            varNames.push_back("_qcdscaleup");
            varNames.push_back("_qcdscaledown");
	  }
	  */
      for(size_t sys=1; sys<varNames.size(); sys++) {
	cout << varNames[sys] << endl;
      }
    }
    size_t nvarsToInclude=varNames.size();
    

    //tree info
    int evStart     = runProcess.getParameter<int>("evStart");
    int evEnd       = runProcess.getParameter<int>("evEnd");
    TString dirname = runProcess.getParameter<std::string>("dirName");


#ifndef YEAR_2017    
    //Lepton scale corrections
    EnergyScaleCorrection_class eScaler_("EgammaAnalysis/ElectronTools/data/ScalesSmearings/Moriond17_74x_pho");     
    eScaler_.doScale=true;
    eScaler_.doSmearings=true;
#endif

    std::string bit_string_stat = "001";
    std::string bit_string_syst = "010";
    std::string bit_string_gain = "100";
    std::bitset<6> bit_stat(bit_string_stat);
    std::bitset<6> bit_syst(bit_string_syst);
    std::bitset<6> bit_gain(bit_string_gain);

    //muon energy scale and uncertainties
    //    TString muscleDir = runProcess.getParameter<std::string>("muscleDir");
    //    gSystem->ExpandPathName(muscleDir);

    //    rochcor2016* muCor2016 = new rochcor2016(); //replace the MuScleFitCorrector we used at run1
    TString muscleDir = runProcess.getParameter<std::string>("muscleDir");
    gSystem->ExpandPathName(muscleDir);
    if(is2016data || is2016MC)	muscleDir += "/RoccoR2016.txt";
    else if(is2017data || is2017MC)	muscleDir += "/RoccoR2017.txt";
    else if(is2018data || is2018MC)	muscleDir += "/RoccoR2018.txt";
    RoccoR rc;
    rc.init(std::string(muscleDir));
    
    //pdf info
    
    //##################################################################################
    //##########################    INITIATING HISTOGRAMS     ##########################
    //##################################################################################

    SmartSelectionMonitor mon;

    TH1F *h=(TH1F*) mon.addHistogram( new TH1F ("eventflow", ";;Events", 8,0,8) );
    h->GetXaxis()->SetBinLabel(1,"Raw");
    if (!runZH) {
      h->GetXaxis()->SetBinLabel(2,"1 lepton");
      h->GetXaxis()->SetBinLabel(3,"Trigger");  
      h->GetXaxis()->SetBinLabel(4,"E_{T}^{miss}>25");   
      h->GetXaxis()->SetBinLabel(5,"M_{T}^{W}>50");  
      h->GetXaxis()->SetBinLabel(6,">=3-jets, >=2 b-tags"); 
      h->GetXaxis()->SetBinLabel(7,"==3b-tags");     
      h->GetXaxis()->SetBinLabel(8,"==4b-tags");    
    } else {
      h->GetXaxis()->SetBinLabel(2,"2 leptons");
      h->GetXaxis()->SetBinLabel(3,"Trigger");
      h->GetXaxis()->SetBinLabel(4,"Z-mass window");
      h->GetXaxis()->SetBinLabel(5,"Z-mass window");  
      h->GetXaxis()->SetBinLabel(6,">=3-jets, >=2 b-tags");   
      h->GetXaxis()->SetBinLabel(7,"==3b-tags");     
      h->GetXaxis()->SetBinLabel(8,"==4b-tags");  
    }

    mon.addHistogram( new TH1F ("jetsMulti", ";;Events", 10,0,10) ); 
 
    mon.addHistogram( new TH1F ("btagEff_b", ";;Events", 200,0,2) );
    mon.addHistogram( new TH1F ("btagEff_c", ";;Events", 200,0,2) );
    mon.addHistogram( new TH1F ("btagEff_udsg", ";;Events", 200,0,2) );

    //generator level plots
    mon.addHistogram( new TH1F( "pileup", ";pileup;Events", 100,-0.5,99.5) );
    
    mon.addHistogram( new TH1F( "higgsMass",";m_{h} [GeV];Events",40,0.,800.) );
    mon.addHistogram( new TH1F( "higgsPt",";p_{T}^{h} [GeV];Events",30,0.,500.));
    //mon.addHistogram( new TH1F( "higgsEta",";#eta (h);Evenets",100,-5,5) );
 
    //Top pt
    mon.addHistogram( new TH1F( "toppt",";#it{p}_{T}^{top} [GeV];Events",100,0.,2000.) );    

    //RECO level, physics objects
    mon.addHistogram( new TH1F( "dR_raw",";#Delta R(SV,b);Events",50,0.,5.));
    mon.addHistogram( new TH1F( "dRlj_raw",";#Delta R(lep,jet);Events",100,0.,5.));

    mon.addHistogram( new TH1F( "lep_pt_raw", ";trailing lepton #it{p}_{T}^{l} [GeV];Events", 50,0.,200.) );  
    mon.addHistogram( new TH1F( "lep_eta_raw",";trailing lepton #eta^{l};Events", 52,-2.6,2.6) );  

    mon.addHistogram( new TH1F( "leadlep_pt_raw", ";Leading lepton #it{p}_{T}^{l} [GeV];Events", 50,0.,200.) );
    mon.addHistogram( new TH1F( "leadlep_eta_raw",";Leading lepton #eta^{l};Events", 52,-2.6,2.6) );
    
    mon.addHistogram( new TH1F( "lep_reliso",";relative Isolation;Events",500,0.,5.) );

    mon.addHistogram( new TH1F( "jet_pt_raw", ";#it{p}_{T} [GeV];Events",50,0.,500.) );
    mon.addHistogram( new TH1F( "softjet_pt_raw", ";#it{p}_{T} [GeV];Events",20,0.,40.) );
    mon.addHistogram( new TH1F( "jet_eta_raw",";jet #eta;Events", 70,-3,3) );
    mon.addHistogram( new TH1F( "jet_phi_raw",";jet #phi;Events", 70,-6,6) );

    mon.addHistogram( new TH1F( "b_discrim"," ;b discriminator;",50,0,1.) );
    mon.addHistogram( new TH1F( "db_discrim"," ;double-b discriminator;",25,-1.,1.) );
    mon.addHistogram( new TH1F( "sd_mass"," ;soft-drop Mass;",60,0.,300.) );
    mon.addHistogram( new TH1F( "pruned_mass"," ;pruned Mass;",60,0.,300.) );
    mon.addHistogram( new TH1F( "softb_ntrk"," ; SV Ntrks;",21,-0.5,21.5) );
    mon.addHistogram( new TH1F( "softb_dxy"," ; SV dxy;",50,0.,20.) );
    mon.addHistogram( new TH1F( "softb_dxyz_signif"," ; SVSIP3D;",50,1.,100.) );
    mon.addHistogram( new TH1F( "softb_cos"," ; SV cos((PV,SV),p_{SV});",25,-1.,1.) );


    //for MC normalization (to 1/pb)
    TH1F* Hcutflow = (TH1F*) mon.addHistogram( new TH1F ("cutflow" , "cutflow" ,6,0,6) ) ;
    
    TH1F *h1 = (TH1F*) mon.addHistogram( new TH1F( "nleptons", ";Lepton multiplicity;Events", 4,0,4) );
    for(int ibin=1; ibin<=h1->GetXaxis()->GetNbins(); ibin++) {
        TString label("");
        if(ibin==h1->GetXaxis()->GetNbins()) label +="#geq";
        else                                label +="=";
        label += (ibin-1);
        h1->GetXaxis()->SetBinLabel(ibin,label);
    }

    TH1F *h2 = (TH1F *)mon.addHistogram( new TH1F("njets_raw",  ";Jet multiplicity (#it{p}_{T}>20 GeV);Events",6,0,6) );
    for(int ibin=1; ibin<=h2->GetXaxis()->GetNbins(); ibin++) {
        TString label("");
        if(ibin==h2->GetXaxis()->GetNbins()) label +="#geq";
        else                                label +="=";
        label += (ibin-1);
        h2->GetXaxis()->SetBinLabel(ibin,label);
    }

    TH1F *h3 = (TH1F *)mon.addHistogram( new TH1F("nbjets_raw",    ";b-tag Jet multiplicity (#it{p}_{T}>20 GeV);Events",6,0,6) );
    for(int ibin=1; ibin<=h3->GetXaxis()->GetNbins(); ibin++) {
        TString label("");
        if(ibin==h3->GetXaxis()->GetNbins()) label +="#geq";
        else                                label +="=";
        label += (ibin-1);
        h3->GetXaxis()->SetBinLabel(ibin,label);
    }

    TH1F *h33 = (TH1F *)mon.addHistogram( new TH1F("nbtags_raw",    ";soft b-tag multiplicity (#it{p}_{T}<20 GeV);Events",6,0,6) ); 
    for(int ibin=1; ibin<=h33->GetXaxis()->GetNbins(); ibin++) {    
      TString label("");   
      if(ibin==h33->GetXaxis()->GetNbins()) label +="#geq";  
      else                                label +="=";  
      label += (ibin-1);   
      h33->GetXaxis()->SetBinLabel(ibin,label);   
    }

    TH2F *h4 = (TH2F *)mon.addHistogram( new TH2F("nbjets_2D", ";AK4 jet multiplicity (#it{p}_{T}>20 GeV); b-Jet multiplicity",5,0,5,5,0,5) );
    for(int ibin=1; ibin<=h4->GetXaxis()->GetNbins(); ibin++) {
      TString label("");
      if(ibin==h4->GetXaxis()->GetNbins()) label +="#geq";
      else                                label +="=";
      label += (ibin-1);
      h4->GetXaxis()->SetBinLabel(ibin,label);
    }
    for(int ibin=1; ibin<=h4->GetYaxis()->GetNbins(); ibin++) {
      TString label("");
      if(ibin==h4->GetYaxis()->GetNbins()) label +="#geq";
      else                                label +="=";
      label += (ibin-1);
      h4->GetYaxis()->SetBinLabel(ibin,label);
    }

    TH1F *h5 = (TH1F *)mon.addHistogram( new TH1F("nsubjets_raw",  ";Subjet multiplicity (#it{p}_{T}>20 GeV);Events",3,0,3) );
    for(int ibin=1; ibin<=h5->GetXaxis()->GetNbins(); ibin++) {
      TString label("");
      if(ibin==h5->GetXaxis()->GetNbins()) label +="#geq";
      else                                label +="=";
      label += (ibin-1);
      h5->GetXaxis()->SetBinLabel(ibin,label);
    }

    // EVent categorization plot
    TH1F *hevt = (TH1F *)mon.addHistogram( new TH1F("evt_cat",  ";(n-btag, n-jet);Events",12,0,12) );
    /*
    hevt->GetXaxis()->SetBinLabel(1,"(0b, 3j)");
    hevt->GetXaxis()->SetBinLabel(2,"(0b, 4j)");
    hevt->GetXaxis()->SetBinLabel(3,"(0b, >=5j)");
    hevt->GetXaxis()->SetBinLabel(4,"(1b, 3j)");
    hevt->GetXaxis()->SetBinLabel(5,"(1b, 4j)");
    hevt->GetXaxis()->SetBinLabel(6,"(1b, >=5j)");
    */
    hevt->GetXaxis()->SetBinLabel(1,"(2b, 3j)");
    hevt->GetXaxis()->SetBinLabel(2,"(2b, 4j)");
    hevt->GetXaxis()->SetBinLabel(3,"(2b, >=5j)");
    hevt->GetXaxis()->SetBinLabel(4,"(3b, 3j)");
    hevt->GetXaxis()->SetBinLabel(5,"(3b, 4j)");
    hevt->GetXaxis()->SetBinLabel(6,"(3b, >=5j)");
    hevt->GetXaxis()->SetBinLabel(7,"(4b, 3j)");
    hevt->GetXaxis()->SetBinLabel(8,"(4b, 4j)");
    hevt->GetXaxis()->SetBinLabel(9,"(4b, >=5j)");
    hevt->GetXaxis()->SetBinLabel(10,"(5b, 3j)");
    hevt->GetXaxis()->SetBinLabel(11,"(5b, 4j)");
    hevt->GetXaxis()->SetBinLabel(12,"(5b, >=5j)");

    //--------------------------------------------------------------------------
    //some strings for tagging histograms:
    const char* astr[] = {"_b1","_b2","_b3","_b4"};
    std::vector<TString> htag(astr, astr+4);
    //--------------------------------------------------------------------------
    
    //btaging efficiency

    //#################################################
    //############# CONTROL PLOTS #####################
    //#################################################
    
    mon.addHistogram( new TH1F( "nvtx_raw",";Vertices;Events",100,0,100) );
    mon.addHistogram( new TH1F( "nvtxwgt_raw",";Vertices;Events",100,0,100) );
    mon.addHistogram( new TH1F( "pfmet",    ";E_{T}^{miss} [GeV];Events", 30,0.,500.) );
    mon.addHistogram( new TH1F( "ht",    ";H_{T} [GeV];Events", 40,0.,800.) );
    mon.addHistogram( new TH1F( "mtw",       ";#it{m}_{T}^{V} [GeV];Events", 60,0.,600.) );
    mon.addHistogram( new TH1F( "ptw",       ";#it{p}_{T}^{V} [GeV];Events",30,0.,500.) );
    mon.addHistogram( new TH1F( "dphiWh", ";#Delta#it{#phi}(#it{V},h);Events", 20,0,TMath::Pi()) );
    mon.addHistogram( new TH1F( "dRave",";#Delta R(b,b)_{ave};Events",50,0.,5.));
    mon.addHistogram( new TH1F( "dmmin",";#Delta m_{b,b}^{min};Events",25,0.,250.));
    mon.addHistogram( new TH1F( "dphijmet", ";|#Delta#it{#phi}(jet,E_{T}^{miss})_{min}|;#jet", 20,0,TMath::Pi()) );
    mon.addHistogram( new TH1F( "dphijmet1", ";|#Delta#it{#phi}(jet1,E_{T}^{miss})|;#jet", 20,0,TMath::Pi()) );   
    mon.addHistogram( new TH1F( "dphijmet12", ";|#Delta#it{#phi}(j12,E_{T}^{miss})|;#jet", 20,0,TMath::Pi()) );           
    mon.addHistogram( new TH1F( "dphilepmet", ";|#Delta#it{#phi}(lep,E_{T}^{miss})|;Events", 20,0,TMath::Pi()) );
    mon.addHistogram( new TH1F( "zmass_raw", ";#it{m}_{ll} [GeV];Events", 80,0,200) );   

    //MVA BDT
    mon.addHistogram( new TH1F( "bdt", ";BDT;Events", 50, -0.5, 0.5) );
    
    // Debugging SFs
    TH2F* musf_id =(TH2F*)mon.addHistogram(new TProfile2D("musfid", ";muon p_{T} (GeV); muon |#eta|",20,0.,400.,10,0,2.5) );    
    TH2F* musf_iso =(TH2F*)mon.addHistogram(new TProfile2D("musfiso", ";muon p_{T} (GeV); muon |#eta|",20,0.,400.,10,0,2.5) );   
    TH2F* musf_trg =(TH2F*)mon.addHistogram(new TProfile2D("musftrg", ";muon p_{T} (GeV); muon |#eta|",20,0.,400.,10,0,2.5) );   

    //##################################################################################
    //########################## STUFF FOR CUTS OPTIMIZATION  ##########################
    //##################################################################################

    TH1F* Hoptim_systs     =  (TH1F*) mon.addHistogram( new TH1F ("optim_systs"    , ";syst;;", nvarsToInclude,0,nvarsToInclude) ) ;
    for(size_t ivar=0; ivar<nvarsToInclude; ivar++){
      Hoptim_systs->GetXaxis()->SetBinLabel(ivar+1, varNames[ivar]);
    }

    std::vector<double> optim_Cuts1_bdt;
    optim_Cuts1_bdt.push_back(-0.6); //add a bin in the shapes with a BDT cut of -0.4
    for(double bdt=-0.50;bdt<0.50;bdt+=0.02) { optim_Cuts1_bdt.push_back(bdt); }

    TH2F* Hoptim_cuts =(TH2F*)mon.addHistogram(new TProfile2D("optim_cut", ";cut index;variable", optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(), 1, 0, 1)) ;
    Hoptim_cuts->GetYaxis()->SetBinLabel(1, "BDT>");
    for(unsigned int index=0;index<optim_Cuts1_bdt.size();index++){ Hoptim_cuts->Fill(index, 0.0, optim_Cuts1_bdt[index]); }
    for(size_t ivar=0; ivar<nvarsToInclude; ivar++){
      mon.addHistogram( new TH2F (TString("bdt_shapes")+varNames[ivar],";cut index;BDT;Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(), 50,-0.5,0.5) );
      if (ivar==0) {         
	mon.addHistogram( new TH2F (TString("higgsMass_shapes")+varNames[ivar],";cut index;m_{h} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(), 40,0.,800.) );
	mon.addHistogram( new TH2F (TString("higgsPt_shapes")+varNames[ivar],";cut index;p_{T}^{h} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(), 30,0.,500.));
	mon.addHistogram( new TH2F (TString("ht_shapes")+varNames[ivar],";cut index;H_{T} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),40,0.,800.) );
	mon.addHistogram( new TH2F (TString("pfmet_shapes")+varNames[ivar],";cut index;E_{T}^{miss} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),50,0.,400.) );
	mon.addHistogram( new TH2F (TString("mtw_shapes")+varNames[ivar],";cut index;#it{m}_{T}^{W} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),40,0.,400.) );
	mon.addHistogram( new TH2F (TString("ptw_shapes")+varNames[ivar],";cut index;#it{p}_{T}^{W} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),30,0.,500.) );
	mon.addHistogram( new TH2F (TString("dphiWh_shapes")+varNames[ivar],";cut index;#Delta#it{#phi}(#it{W},h);Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),20,0,TMath::Pi()) );
	mon.addHistogram( new TH2F (TString("dRave_shapes")+varNames[ivar],";cut index;#Delta R(b,b)_{ave};Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),50,0.,5.));
	mon.addHistogram( new TH2F (TString("dmmin_shapes")+varNames[ivar],";cut index;#Delta m_{b,b}^{min};Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),25,0.,250.));
	mon.addHistogram( new TH2F (TString("dphijmet_shapes")+varNames[ivar],";cut index;#Delta#it{#phi}(jet,E_{T}^{miss})_{min}|;Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),20,0,TMath::Pi()) );
	mon.addHistogram( new TH2F (TString("lep_pt_raw_shapes")+varNames[ivar],";cut index;lepton p_{T} [GeV];Events",optim_Cuts1_bdt.size(),0,optim_Cuts1_bdt.size(),80,0,200.));
      }
    }

    //##################################################################################
    //#############         GET READY FOR THE EVENT LOOP           #####################
    //##################################################################################

    TFile *f_CSVwgt_HF = new TFile(), *f_CSVwgt_LF = new TFile();
    if(nMethod == 2) {
      f_CSVwgt_HF = new TFile((std::string(std::getenv("CMSSW_BASE")) + "/src/UserCode/bsmhiggs_fwk/data/weights/" + inputFileHF).c_str());
      f_CSVwgt_LF = new TFile((std::string(std::getenv("CMSSW_BASE")) + "/src/UserCode/bsmhiggs_fwk/data/weights/" + inputFileLF).c_str());
      fillCSVhistos(f_CSVwgt_HF, f_CSVwgt_LF);
    }
    //open the file and get events tree
    DataEvtSummaryHandler summaryHandler_;

    TFile *file = TFile::Open(url);
    printf("Looping on %s\n",url.Data());
    if(file==0) { return -1; printf("file is 0"); }
    
    if(file->IsZombie()) return -1;
    if( !summaryHandler_.attachToTree( (TTree *)file->Get(dirname) ) ) {
        file->Close();
        return -1;
    }


    //check run range to compute scale factor (if not all entries are used)
    const Int_t totalEntries= summaryHandler_.getEntries();
    float rescaleFactor( evEnd>0 ?  float(totalEntries)/float(evEnd-evStart) : -1 );
    if(evEnd<0 || evEnd>summaryHandler_.getEntries() ) evEnd=totalEntries;
    if(evStart > evEnd ) {
        file->Close();
        return -1;
    }

    //MC normalization (to 1/pb)
    double xsecWeight = 1.0;
    float cnorm=1.0;
    if (isMC) {
      //xsecWeight = 0.; //disable MC sample if not present in the map
      
      double totalNumberofEvents(0.);
      
      TH1F* nevtH = (TH1F *) file->Get("mainNtuplizer/nevents");
      totalNumberofEvents = nevtH->GetBinContent(1);
      TH1F* posH = (TH1F *) file->Get("mainNtuplizer/n_posevents");
      TH1F* negH = (TH1F *) file->Get("mainNtuplizer/n_negevents");
      if(posH && negH) cnorm = posH->GetBinContent(1) - negH->GetBinContent(1);
      if(rescaleFactor>0) cnorm /= rescaleFactor;
      printf("cnorm = %f and totalNumberOfEvents= %f\n", cnorm, totalNumberofEvents);
      //xsecWeight=xsec/totalNumberofEvents;
      xsecWeight=xsec/cnorm; // effective luminosity}
      /*
      std::map<std::string, int> xsec_map = mStat;
      //std::string myproc = proc.Data();
      //std::cout << "Runnin process " << myproc << std::endl;
      std::map<std::string, int>::iterator it;
      for ( it = xsec_map.begin(); it != xsec_map.end(); it++ ) {
        if (it->first == proc.Data()) {
          xsecWeight = (xsec/(float)it->second);
          //totalNumberofEvents = it->second;
          std::cout << "weight = " << (xsecWeight*35866.9) << std::endl;
        }
      }
      */
      //float pereventwgt=(xsecWeight*35866.9);
      //printf("\n Running process with xSec = %f , and totalNumEvents = %d  . Per event weight is (L=35.9 fb-1): %f \n\n",
      //xsec, totalNumberofEvents, pereventwgt );
    }
    //Hcutflow->SetBinContent(1,cnorm);

    //pileup weighting
    TString PU_Central = runProcess.getParameter<std::string>("PU_Central");
    gSystem->ExpandPathName(PU_Central);
    cout << "Loading PU weights Central: " << PU_Central << endl;
    TFile *PU_Central_File = TFile::Open(PU_Central);
    
    TString PU_Up = runProcess.getParameter<std::string>("PU_Up");
    gSystem->ExpandPathName(PU_Up);
    cout << "Loading PU weights Up: " << PU_Up << endl;
    TFile *PU_Up_File = TFile::Open(PU_Up);
    //    TH1F* weight_pileup_Up = (TH1F *) PU_Up_File->Get("pileup");

    TString PU_Down = runProcess.getParameter<std::string>("PU_Down");
    gSystem->ExpandPathName(PU_Down);
    cout << "Loading PU weights Down: " << PU_Down << endl;
    TFile *PU_Down_File = TFile::Open(PU_Down);
    //  TH1F* weight_pileup_Down = (TH1F *) PU_Down_File->Get("pileup");


    TH1F* PU_intended = (TH1F *) PU_Central_File->Get("pileup"); // Data pileup distribution
    TH1F* PU_intended_Up = (TH1F *) PU_Up_File->Get("pileup"); 
    TH1F* PU_intended_Down = (TH1F *) PU_Down_File->Get("pileup"); 

    TH1F* PU_generated=NULL; // MC pileup distribution 

    TH1F* PU_weight=new TH1F("hPUweight","",100,0,100);
    TH1F* PU_weight_Up=new TH1F("hPUweight_Up","",100,0,100);   
    TH1F* PU_weight_Down=new TH1F("hPUweight_Down","",100,0,100);     
    
    if (isMC) {
      PU_generated = (TH1F*)file->Get("mainNtuplizer/pileuptrue");
      
      PU_intended->Scale(1./PU_intended->Integral());
      PU_intended_Up->Scale(1./PU_intended_Up->Integral());      
      PU_intended_Down->Scale(1./PU_intended_Down->Integral());      

      //      TH1F* PUnorm = PU_intended;
      PU_generated->Scale(1./PU_generated->Integral());
      TH1F *PUgen = PU_generated;
      
      TH1F* Quotient = PU_intended; Quotient->Divide(PUgen); 
      TH1F* Quotient_Up = PU_intended_Up; Quotient_Up->Divide(PUgen);   //Up
      TH1F* Quotient_Down = PU_intended_Down; Quotient_Down->Divide(PUgen);   //Down

      for(int ibin=0; ibin<100; ++ibin){

        float x = Quotient->GetBinContent(ibin);
	float x_up = Quotient_Up->GetBinContent(ibin);  //up
	float x_down = Quotient_Down->GetBinContent(ibin);  //down

        PU_weight->SetBinContent(ibin,x);
	PU_weight_Up->SetBinContent(ibin,x_up); //up
	PU_weight_Down->SetBinContent(ibin,x_down); //down

        if ( verbose ) printf("pu = %3d has weight = %7.3f \n",ibin,x);

      }
    }//is MC
    
    //event categorizer
    EventCategory eventCategoryInst_Wh(1); //WH channel
    EventCategory eventCategoryInst_Zh(2); //ZH channel       

    EventCategory eventCategoryPlot(3);   

    //Lepton scale factors
    LeptonEfficiencySF lepEff;

    // e TRG eff SF 2Dhisto
    TString eTRG_sf = runProcess.getParameter<std::string>("ele_trgSF");
    gSystem->ExpandPathName(eTRG_sf);
    TFile *E_TRG_SF_file = TFile::Open(eTRG_sf);
    TH2F*  E_TRG_SF_h1 = (TH2F*) E_TRG_SF_file->Get("Ele27_WPTight_Gsf");
    // TH2F* E_TRG_SF_h2 = (TH2F*) E_TRG_SF_file->Get("Ele25_eta2p1_WPTight_Gsf");   
    // e RECO eff SF 2Dhisto          
    TString eRECO_sf = runProcess.getParameter<std::string>("ele_recoSF"); 
    gSystem->ExpandPathName(eRECO_sf); 
    TFile *E_RECO_SF_file = TFile::Open(eRECO_sf);          
    TH2F* E_RECO_SF_h = (TH2F*) E_RECO_SF_file->Get("EGamma_SF2D");
    // e cut based TightID eff SF 2Dhisto          
    TString eTIGHTID_sf = runProcess.getParameter<std::string>("ele_TightIdSF");  
    gSystem->ExpandPathName(eTIGHTID_sf);     
    TFile *E_TIGHTID_SF_file = TFile::Open(eTIGHTID_sf);  
    TH2F* E_TIGHTID_SF_h = (TH2F*) E_TIGHTID_SF_file->Get("EGamma_SF2D");    
    
    // mu TRG eff SF 2Dhisto
    TString muTRG_sf = runProcess.getParameter<std::string>("mu_trgSF"); 
    gSystem->ExpandPathName(muTRG_sf);   
    TFile *MU_TRG_SF_file = TFile::Open(muTRG_sf);  
    TH2F* MU_TRG_SF_h = new TH2F();
    if(is2018data || is2018MC)
        // https://twiki.cern.ch/twiki/bin/viewauth/CMS/MuonReferenceEffs2018, run < 316361
        MU_TRG_SF_h = (TH2F*) MU_TRG_SF_file->Get("IsoMu24_PtEtaBins/pt_abseta_ratio");
    else if(is2017data || is2017MC)
        MU_TRG_SF_h = (TH2F*) MU_TRG_SF_file->Get("IsoMu27_PtEtaBins/pt_abseta_ratio");
    else if(is2016data || is2016MC)
        MU_TRG_SF_h = (TH2F*) MU_TRG_SF_file->Get("IsoMu24_OR_IsoTkMu24_PtEtaBins/pt_abseta_ratio");

    TString muTRG_sf2 = runProcess.getParameter<std::string>("mu_trgSF2");  
    gSystem->ExpandPathName(muTRG_sf2);         
    TFile *MU_TRG_SF_file2 = TFile::Open(muTRG_sf2);
    TH2F* MU_TRG_SF_h2 = new TH2F(); 
    if(is2018data || is2018MC)
        // https://twiki.cern.ch/twiki/bin/viewauth/CMS/MuonReferenceEffs2018, run >= 316361
        MU_TRG_SF_h2 = (TH2F*) MU_TRG_SF_file->Get("IsoMu24_PtEtaBins/pt_abseta_ratio");
    else if(is2017data || is2017MC)
        MU_TRG_SF_h2 = (TH2F*) MU_TRG_SF_file->Get("IsoMu27_PtEtaBins/pt_abseta_ratio");
    else if(is2016data || is2016MC)
        MU_TRG_SF_h2 = (TH2F*) MU_TRG_SF_file->Get("IsoMu24_OR_IsoTkMu24_PtEtaBins/pt_abseta_ratio");

     // mu ID SFs
    TString muID_sf = runProcess.getParameter<std::string>("mu_idSF"); 
    gSystem->ExpandPathName(muID_sf);   
    TFile *MU_ID_SF_file = TFile::Open(muID_sf);  
    TH2F* MU_ID_SF_h = new TH2F();
    if(is2018data || is2018MC)
      // https://twiki.cern.ch/twiki/bin/viewauth/CMS/MuonReferenceEffs2018
        MU_ID_SF_h = (TH2F*) MU_ID_SF_file->Get("NUM_TightID_DEN_TrackerMuons_pt_abseta");
    else if(is2017data || is2017MC)
        MU_ID_SF_h = (TH2F*) MU_ID_SF_file->Get("NUM_TightID_DEN_genTracks_pt_abseta");
    else if(is2016data || is2016MC)
        MU_ID_SF_h = (TH2F*) MU_ID_SF_file->Get("MC_NUM_TightID_DEN_genTracks_PAR_pt_eta/pt_abseta_ratio");
    
    TString muID_sf2 = runProcess.getParameter<std::string>("mu_idSF2");    
    gSystem->ExpandPathName(muID_sf2); 
    TFile *MU_ID_SF_file2 = TFile::Open(muID_sf2);  
    TH2F* MU_ID_SF_h2 = new TH2F();
    if(is2018data || is2018MC)
      // https://twiki.cern.ch/twiki/bin/viewauth/CMS/MuonReferenceEffs2018
        MU_ID_SF_h2 = (TH2F*) MU_ID_SF_file->Get("NUM_TightID_DEN_TrackerMuons_pt_abseta");
    else if(is2017data || is2017MC)
        MU_ID_SF_h2 = (TH2F*) MU_ID_SF_file->Get("NUM_TightID_DEN_genTracks_pt_abseta");
    else if(is2016data || is2016MC)
        MU_ID_SF_h2 = (TH2F*) MU_ID_SF_file->Get("MC_NUM_TightID_DEN_genTracks_PAR_pt_eta/pt_abseta_ratio");

    // mu ISO SFs
    TString muISO_sf = runProcess.getParameter<std::string>("mu_isoSF"); 
    gSystem->ExpandPathName(muISO_sf);   
    TFile *MU_ISO_SF_file = TFile::Open(muISO_sf);  
    TH2F* MU_ISO_SF_h = new TH2F();
    if(is2018data || is2018MC)
      // https://twiki.cern.ch/twiki/bin/viewauth/CMS/MuonReferenceEffs2018
        MU_ISO_SF_h = (TH2F*) MU_ISO_SF_file->Get("NUM_TightRelIso_DEN_TightIDandIPCut_pt_abseta");
    else if(is2017data || is2017MC)
        MU_ISO_SF_h = (TH2F*) MU_ISO_SF_file->Get("NUM_TightRelIso_DEN_TightIDandIPCut_pt_abseta");
    else if(is2016data || is2016MC)
        MU_ISO_SF_h = (TH2F*) MU_ISO_SF_file->Get("TightISO_TightID_pt_eta/pt_abseta_ratio");

    TString muISO_sf2 = runProcess.getParameter<std::string>("mu_isoSF2");        
    gSystem->ExpandPathName(muISO_sf2);       
    TFile *MU_ISO_SF_file2 = TFile::Open(muISO_sf2);   
    TH2F* MU_ISO_SF_h2 = new TH2F();
    if(is2018data || is2018MC)
      // https://twiki.cern.ch/twiki/bin/viewauth/CMS/MuonReferenceEffs2018
        MU_ISO_SF_h2 = (TH2F*) MU_ISO_SF_file->Get("NUM_TightRelIso_DEN_TightIDandIPCut_pt_abseta");
    else if(is2017data || is2017MC)
        MU_ISO_SF_h2 = (TH2F*) MU_ISO_SF_file->Get("NUM_TightRelIso_DEN_TightIDandIPCut_pt_abseta");
    else if(is2016data || is2016MC)
        MU_ISO_SF_h2 = (TH2F*) MU_ISO_SF_file->Get("TightISO_TightID_pt_eta/pt_abseta_ratio");

    //####################################################################################################################
    //###########################################           BTaggingMC         ###########################################
    //####################################################################################################################
    TH2F* btagEffLoose_b = new TH2F(), *btagEffMedium_b = new TH2F();
    TH2F* btagEffLoose_c = new TH2F(), *btagEffMedium_c = new TH2F();
    TH2F* btagEffLoose_udsg = new TH2F(), *btagEffMedium_udsg = new TH2F();
    TFile *btagfile = new TFile();
    if(isMC && nMethod==1){
      TString btagfilename = btagDir + "/" + proc + "_BTaggEff.root";
      btagfile = TFile::Open(btagfilename);
      if(btagfile->IsZombie() || !btagfile->IsOpen()) {std::cout<<"Error, cannot open file: "<<btagfilename<<std::endl;return -1;}
      btagEffLoose_b = (TH2F *)btagfile->Get("Loose_efficiency_b");
      btagEffLoose_b->SetDirectory(0); // to decouple it from the open file direcotry
      btagEffLoose_c = (TH2F *)btagfile->Get("Loose_efficiency_c");
      btagEffLoose_c->SetDirectory(0);
      btagEffLoose_udsg = (TH2F *)btagfile->Get("Loose_efficiency_udsg");
      btagEffLoose_udsg->SetDirectory(0);
      
      btagEffMedium_b = (TH2F *)btagfile->Get("Medium_efficiency_b");
      btagEffMedium_b->SetDirectory(0); // to decouple it from the open file direcotry
      btagEffMedium_c = (TH2F *)btagfile->Get("Medium_efficiency_c");
      btagEffMedium_c->SetDirectory(0);
      btagEffMedium_udsg = (TH2F *)btagfile->Get("Medium_efficiency_udsg");
      btagEffMedium_udsg->SetDirectory(0);
      btagfile->Close();
    }
    
    //####################################################################################################################
    //###########################################           Z Pt SFs         ###########################################
    //####################################################################################################################
    TH1F *zptSF_3j = new TH1F(), *zptSF_4j = new TH1F(), *zptSF_5j = new TH1F();
    if(!reweightDYZPt && isMC_DY && !dtag.Contains("amcNLO")){ // apply Z Pt weights on LO DY samples
      TString zptfilename;
      if(is2016MC) zptfilename = zptDir + "/" +"DYSF_2016.root";
      else if(is2017MC) zptfilename = zptDir + "/" +"DYSF_2017.root";
      else if(is2018MC) zptfilename = zptDir + "/" +"DYSF_2018.root";
      TFile *zptfile = TFile::Open(zptfilename);
      if(zptfile->IsZombie() || !zptfile->IsOpen()) {std::cout<<"Error, cannot open file: "<< zptfilename<<std::endl;return -1;}
      zptSF_3j = (TH1F *)zptfile->Get("3jets_sf");
      zptSF_3j->SetDirectory(0);
      zptSF_4j = (TH1F *)zptfile->Get("4jets_sf");
      zptSF_4j->SetDirectory(0);
      zptSF_5j = (TH1F *)zptfile->Get("5+jets_sf");
      zptSF_5j->SetDirectory(0);
      zptfile->Close();
    }


    //####################################################################################################################
    //###########################################           MVAHandler         ###########################################
    //####################################################################################################################
    //construct MVA out put file name
    TString mvaout = TString ( runProcess.getParameter<std::string>("outdir") ) + "/mva_" + outFileUrl + ".root";
    MVAHandler myMVAHandler_;
    if (runMVA) { myMVAHandler_.initTree(mvaout); }

    //####################################################################################################################
    //###########################################           TMVAReader         ###########################################
    //####################################################################################################################
    
    std::string chpath = "WHaa4bMVA/";
    if (runZH) chpath =  "ZHaa4bMVA/";

    std::string mva_year = "mva2016/";
    if(is2017MC || is2017data) mva_year = "mva2017/";
    else if(is2018MC || is2018data) mva_year = "mva2018/";

    TMVAReader myTribTMVAReader;
    myTribTMVAReader.InitTMVAReader();
    std::string TribMVA_xml_path = std::string(std::getenv("CMSSW_BASE"))+"/src/UserCode/bsmhiggs_fwk/data/mva/"+mva_year+chpath+"Haa4bSBClassificationTribMVA_BDT.weights.xml"; // ---> use a signle BDT
    myTribTMVAReader.SetupMVAReader( "Haa4bSBClassificationTribMVA", TribMVA_xml_path );

    TMVAReader myQuabTMVAReader;
    myQuabTMVAReader.InitTMVAReader();
    std::string QuabMVA_xml_path = std::string(std::getenv("CMSSW_BASE"))+"/src/UserCode/bsmhiggs_fwk/data/mva/"+mva_year+chpath+"Haa4bSBClassificationQuabMVA_BDT.weights.xml"; 
    myQuabTMVAReader.SetupMVAReader( "Haa4bSBClassificationQuabMVA", QuabMVA_xml_path );
    
    //####################################################################################################################
    //###########################################           EVENT LOOP         ###########################################
    //####################################################################################################################

    //loop on all the events
    int treeStep = (evEnd-evStart)/50;
    if(treeStep==0)treeStep=1;
    DuplicatesChecker duplicatesChecker;
    int nDuplicates(0);
    printf("Progressing Bar     :0%%       20%%       40%%       60%%       80%%       100%%\n");
    printf("Scanning the ntuple :");

    for( int iev=evStart; iev<evEnd; iev++) 
    {
        if((iev-evStart)%treeStep==0) {
          printf("."); fflush(stdout);
        }

        if ( verbose ) printf("\n\n Event info %3d: \n",iev);

        //##############################################   EVENT LOOP STARTS   ###########################################
        //load the event content from tree
        summaryHandler_.getEntry(iev);
        DataEvtSummary_t &ev=summaryHandler_.getEvent();
        if(!isMC && duplicatesChecker.isDuplicate( ev.run, ev.lumi, ev.event) ) {
            nDuplicates++;
            cout << "nDuplicates: " << nDuplicates << endl;
            continue;
        }
	if(is2018data) afterRun319077 = (ev.run > 319077);

        // add PhysicsEvent_t class, get all tree to physics objects
        PhysicsEvent_t phys=getPhysicsEventFrom(ev); 

	//	std::vector<TString> tags(1,"all");

	//genWeight
        float genWeight = 1.0;
        if (isMC) {
          if(ev.genWeight<0) { genWeight = -1.0; }
        }
        //systematical weight
        float weight = 1.0; //xsecWeight;
        if(isMC) 
        {
          weight *= genWeight;
          //Here is the tricky part.,... rewrite xsecWeight for WJets/WXJets and DYJets/DYXJets
          if( isMC_WJets )
	    { xsecWeight = xsecWeightCalculator::xsecWeightCalcLHEJets(0, ev.lheNJets, is2016MC<<0|is2017MC<<1|is2018MC<<2); }
	  else if( isMC_DY && !dtag.Contains("amcNLO") ) {
	    if (string(url.Data()).find("10to50")  != string::npos)
	      {
	  	if (is2016MC) xsecWeight = xsecWeightCalculator::xsecWeightCalcLHEJets(1, ev.lheNJets, is2016MC<<0|is2017MC<<1|is2018MC<<2);
	      }
	    else
	      { xsecWeight = xsecWeightCalculator::xsecWeightCalcLHEJets(2, ev.lheNJets, is2016MC<<0|is2017MC<<1|is2018MC<<2); }
	  }
          weight *= xsecWeight; 
        }

	// Extract Z pt reweights from LO and NLO DY samples
	float zpt = -1;
	if(isMC_DY){
	  PhysicsObjectCollection &genparticles = phys.genparticles;
	  for (auto & genparticle : genparticles) {
//	    printf("Parton : ID=%6d, m=%5.1f, momID=%6d : pt=%6.1f, status=%d\n",
//		   genparticle.id,
//		   genparticle.mass(),
//		   genparticle.momid,
//		   genparticle.pt(),
//		   genparticle.status
//		   );
	      
	    if(genparticle.id==23) {  
	      if(zpt<0)
	        zpt = genparticle.pt();
	      else
		std::cout << "Found multiple Z particles in event: " << iev << std::endl;
	    }
	  }
	}

	// Apply Top pt-reweighting
	double top_wgt(1.0);    

	if(reweightTopPt && isMC_ttbar){
	  PhysicsObjectCollection &partons = phys.genpartons;
	  double SFtop(0.);
	  double SFantitop(0.);

	  int itop(0);
	  for (auto & top : partons) {

	    printf("Parton : ID=%6d, m=%5.1f, momID=%6d : pt=%6.1f, status=%d\n",
		   top.id,
		   top.mass(),
		   top.momid,
		   top.pt(),
		   top.status
		   );

	    if (top.id==6 && top.status==62) {
	      SFtop=exp(0.0615-0.0005*top.pt());
	      itop++;

	      mon.fillHisto("toppt","top",top.pt(),weight);
	    }
	    if (top.id==-6 && top.status==62) {
	      SFantitop=exp(0.0615-0.0005*top.pt());
	      itop++;

	      mon.fillHisto("toppt","antitop",top.pt(),weight);
	    }
	  }
  
	  if (itop<2) { 
	    printf("Did not found tt pair!!\n"); 
	  } else if (itop==2) {
	    top_wgt=sqrt(SFtop*SFantitop);
	  } else {
	    printf("More than 2 top particles found. Please check\n");
	  }
    
	  //printf("weight= %3f and top weight= %3f\n",weight,top_wgt);
	  weight *= top_wgt;
	  //printf("Final weight is : %3f\n\n",weight);
	}
	
	
	// All: "Raw" (+Trigger)
	mon.fillHisto("eventflow","e",0,weight); 
	mon.fillHisto("eventflow","mu",0,weight); 
	mon.fillHisto("eventflow","ee",0,weight);  
	mon.fillHisto("eventflow","mumu",0,weight); 
	mon.fillHisto("eventflow","emu",0,weight);   

        //only take up and down from pileup effect
        double TotalWeight_plus = 1.0;
        double TotalWeight_minus = 1.0;

        if(isMC) mon.fillHisto("pileup", "raw", ev.ngenTruepu, 1.0);
        
        float puWeight(1.0);
        if(isMC) {
          puWeight = getSFfrom1DHist(ev.ngenTruepu+1, PU_weight) ;
          if ( verbose ) printf("pu = %3d has weight = %7.3f \n",ev.ngenTruepu,puWeight);
          weight *= puWeight;
          TotalWeight_plus  *= getSFfrom1DHist(ev.ngenTruepu+1, PU_weight_Up);
          TotalWeight_minus *= getSFfrom1DHist(ev.ngenTruepu+1, PU_weight_Down);
        }
        
        Hcutflow->Fill(1,genWeight);
        Hcutflow->Fill(2,xsecWeight);
        Hcutflow->Fill(3,puWeight);
        Hcutflow->Fill(4,weight);
        Hcutflow->Fill(3,weight*TotalWeight_minus);
        Hcutflow->Fill(4,weight*TotalWeight_plus);

        //add PhysicsEvent_t class, get all tree to physics objects
        //PhysicsEvent_t phys=getPhysicsEventFrom(ev);

        // FIXME need to have a function: loop all leptons, find a Z candidate,
        // can have input, ev.mn, ev.en
        // assign ee,mm,emu channel
        // check if channel name is consistent with trigger
        // store dilepton candidate in lep1 lep2, and other leptons in 3rdleps


        bool hasMMtrigger = ev.triggerType & 0x1;
        bool hasMtrigger  = (ev.triggerType >> 1 ) & 0x1;
        bool hasEEtrigger = (ev.triggerType >> 2 ) & 0x1;
        // type 3 is high-pT eeTrigger (safety)
	bool hasHighPtEtrigger  = (ev.triggerType >> 3 ) & 0x1;
        bool hasEtrigger  = (ev.triggerType >> 4 ) & 0x1;
        bool hasEMtrigger = (ev.triggerType >> 5 ) & 0x1;
	bool hasMtrigger2 = (ev.triggerType >> 6 ) & 0x1;
        bool hasEtrigger2  = (ev.triggerType >> 7 ) & 0x1;
        bool hasEEtrigger2  = (ev.triggerType >> 8 ) & 0x1;
	bool hasMMtrigger2 = (ev.triggerType >> 9 ) & 0x1;
	bool hasEMtrigger2 = (ev.triggerType >> 10) & 0x1;
        // Follow the EG POG recommendation, require pass HLT_Ele32_WPTight_L1DoubleEG and HLT_Ele35_WPTight at the same time when is 2017 RunB or RunC
        // https://indico.cern.ch/event/662751/contributions/2778365/attachments/1561439/2458438/egamma_workshop_triggerTalk.pdf
        //if(is2017BCdata) {hasEtrigger = hasEtrigger && hasEtrigger2;printf("hasEtrigger:%d\n",hasEtrigger);}
        //#########################################################################
        //#####################      Objects Selection       ######################
        //######################################################################### 

        //
        // MET ANALYSIS
        //
        //apply Jet Energy Resolution corrections to jets (and compute associated variations on the MET variable)
	std::vector<PhysicsObjectJetCollection> variedJets;
	LorentzVectorCollection &variedMET = phys.variedMet;

	LorentzVector metP4 = variedMET[0];
	//	LorentzVector &metP4 = phys.met;
	//	METUtils::computeVariation(phys.jets, phys.leptons, (usemetNoHF ? phys.metNoHF : phys.met), variedJets, variedMET, totalJESUnc, ( (is2017data||is2017data) << 0 ) | ( (is2018data||is2018data) << 1));

	PhysicsObjectJetCollection &corrJets = phys.jets; 
	//        PhysicsObjectFatJetCollection &fatJets = phys.fatjets;
        PhysicsObjectSVCollection &secVs = phys.svs;

        //
        // LEPTON ANALYSIS
        //
        PhysicsObjectLeptonCollection &leps = phys.leptons;
	
	std::map<string, std::vector<PhysicsObject_Lepton> > selLeptonsVar;
	//	std::vector<std::pair<int,LorentzVector> > goodLeptons;
	std::vector<PhysicsObject_Lepton> allLeptons;  

	LorentzVector muDiff(0,0,0,0);
	LorentzVector elDiff(0,0,0,0);
	LorentzVector elDiff_forMET(0,0,0,0);

	int nExtraLeptons(0);
	std::vector<LorentzVector> extraLeptons;
	std::vector<LorentzVector> vetoLeptons; // ---> Use this collection to remove jet-lepton overlaps for e/mu below 30(25) GeV. 

	float lep_threshold(10.);
	float eta_threshold=2.5;

	eleinHEM = false;
	for (auto &ilep : leps) {

	  int lepid = ilep.id;
	  if (abs(lepid)==13) eta_threshold=2.4;
	  if (fabs(ilep.eta())>eta_threshold) continue;
 
	  bool hasTightIdandIso(true);
	  // for 2017 and 2018 analysis, use the correct Iso Cut value from the twiki:
	  // https://twiki.cern.ch/twiki/bin/view/CMS/CutBasedElectronIdentificationRun2

	  if (abs(lepid)==11) {
	    lep_threshold=ele_threshold_;
	    if(!runQCD) hasTightIdandIso = (ilep.passIdEl && ilep.passIsoEl);
	    //            else hasTightIdandIso = ilep.passIdEl;
	    if ( (ilep.passIdEl) && (ilep.pt()>lep_threshold) ) allLeptons.push_back(ilep);
	  } else if (abs(lepid)==13) {
	    lep_threshold=mu_threshold_;
            if(!runQCD)hasTightIdandIso = (ilep.passIdMu && ilep.passIsoMu);          
	    //	    else hasTightIdandIso = ilep.passIdMu;
	    if ( (ilep.passIdMu) && (ilep.pt()>lep_threshold) ) allLeptons.push_back(ilep);  
	  } else continue;

	  bool hasExtraLepton(false);

	  if ( hasTightIdandIso && (ilep.pt()>lep_threshold) ) {

	    if(abs(lepid)==11) { // ele scale corrections
	      if( !eleinHEM && ilep.pt()>25 && 
		  -1.57<ilep.phi() && ilep.phi()<-0.87 &&
		  -3.0<ilep.eta() && ilep.eta()<-1.4) eleinHEM = true;
	      double et = ilep.en_cor_en / cosh(fabs(ilep.en_EtaSC));
	      //double et = ilep.en_en / cosh(fabs(ilep.en_EtaSC));

	      elDiff -= ilep;    
	      if(fabs(ilep.en_EtaSC) < 1.479) elDiff_forMET -= ilep*0.006;
	      else elDiff_forMET -= ilep*0.015; 
	      
	      if (isMC) {
		double sigma=0.0;
#ifdef YEAR_2017
		//https://twiki.cern.ch/twiki/bin/viewauth/CMS/EgammaMiniAODV2#2017_MiniAOD_V2
		sigma = ilep.en_enSigmaValue;
#else
		sigma = eScaler_.getSmearingSigma(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et,ilep.en_gainSeed,0,0);
#endif
		//Now smear the MC energy
		TRandom3 *rgen_ = new TRandom3(0);
		double smearValue = rgen_->Gaus(1, sigma) ;
		delete rgen_;
		//TLorentzVector p4        
		ilep.SetPxPyPzE(ilep.Px()*smearValue, ilep.Py()*smearValue, ilep.Pz()*smearValue, ilep.E()*smearValue); 
	      } else {
		double scale_corr=1.0;
#ifdef YEAR_2017
		scale_corr = ilep.en_enScaleValue;
#else
		scale_corr = eScaler_.ScaleCorrection(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et,ilep.en_gainSeed); 
#endif
		//TLorentzVector p4
		ilep.SetPxPyPzE(ilep.Px()*scale_corr, ilep.Py()*scale_corr, ilep.Pz()*scale_corr, ilep.E()*scale_corr); 
	      }

	      elDiff += ilep;    
	      if(fabs(ilep.en_EtaSC) < 1.479) elDiff_forMET += ilep*0.006;     
	      else elDiff_forMET += ilep*0.015;   

	    } 
	    //https://twiki.cern.ch/twiki/bin/view/CMS/RochcorMuon#Available_correction_in_the_prod
	    else if(abs(lepid)==13){
	      int charge = lepid > 0 ? 1 : -1;
	      double sf;
	      if(isMC){TRandom3* rand_ = new TRandom3(0);float randn = rand_->Uniform(1.);sf = rc.kSmearMC(charge, ilep.pt(), ilep.eta(), ilep.phi(), ilep.mn_trkLayersWithMeasurement, randn, 0, 0);}
	      else {sf = rc.kScaleDT(charge, ilep.pt(), ilep.eta(), ilep.phi(), 0, 0);}
	      muDiff -= ilep;
//	      printf("Muon P4 (before roch): px=%f, py=%f, pz=%f, e=%f\n",ilep.Px(),ilep.Py(),ilep.Pz(),ilep.E());
	      ilep.SetPxPyPzE(ilep.Px()*sf,ilep.Py()*sf,ilep.Pz()*sf,ilep.E()*sf); muDiff += ilep;
//	      printf("Muon P4 (AFTER roch): px=%f, py=%f, pz=%f, e=%f\n\n",ilep.Px(),ilep.Py(),ilep.Pz(),ilep.E());
	    }
	    /*
	    else if (abs(lepid)==13) { // mu scale corrections    
	      if(muCor2016){
		float qter =1.0;
		int ntrk = ilep.mn_trkLayersWithMeasurement;

		TLorentzVector p4(ilep.Px(),ilep.Py(),ilep.Pz(),ilep.E());
		muDiff -= ilep;
		// printf("Muon P4 (before roch): px=%f, py=%f, pz=%f, e=%f\n",p4.Px(),p4.Py(),p4.Pz(),p4.E());
		if (isMC) { muCor2016->momcor_mc(p4, lepid<0 ? -1 :1, ntrk, qter);}
		else { muCor2016->momcor_data(p4, lepid<0 ? -1 :1, 0, qter); }

		ilep.SetPxPyPzE(p4.Px(),p4.Py(),p4.Pz(),p4.E()); muDiff += ilep;
		// printf("Muon P4 (AFTER roch): px=%f, py=%f, pz=%f, e=%f\n\n",ilep.Px(),ilep.Py(),ilep.Pz(),ilep.E());
	      }
	    }
	    */

	    // Systematic variations for e/mu
	    for(unsigned int ivar=0;ivar<eleVarNames.size();ivar++){
	      // only run Systematics in MC samples
	      if(!isMC && ivar>0) continue;
	      // do not run systs. for QCD
	      
	      if (abs(lepid)==11) {
		double et = ilep.en_cor_en / cosh(fabs(ilep.en_EtaSC)); 

		if(ivar==1) { //stat electron up
		  double error_scale=0.0;
#ifdef YEAR_2017
		  error_scale = ilep.en_enScaleStatUp/ilep.E()-1;
//      printf("Electron stat up error scale: %f\n",error_scale);
#else
		  error_scale = eScaler_.ScaleCorrectionUncertainty(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,bit_stat);
#endif
		  ilep.SetPxPyPzE(ilep.Px()*(1.+error_scale), ilep.Py()*(1.+error_scale), ilep.Pz()*(1.+error_scale), ilep.E()*(1.+error_scale));   
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);
		}if(ivar==2) { //stat electron down
		  double error_scale=0.0;
#ifdef YEAR_2017
		  error_scale = ilep.en_enScaleStatDown/ilep.E()-1;
//      printf("Electron stat down error scale: %f\n",error_scale);
#else
		  error_scale = eScaler_.ScaleCorrectionUncertainty(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,bit_stat); 
#endif
		  ilep.SetPxPyPzE(ilep.Px()*(1.-error_scale), ilep.Py()*(1.-error_scale), ilep.Pz()*(1.-error_scale), ilep.E()*(1.-error_scale)); 
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);   
		}if(ivar==3) { //systematic electron up
		  double error_scale=0.0;
#ifdef YEAR_2017
		  error_scale = ilep.en_enScaleSystUp/ilep.E()-1;
//      printf("Electron sys up error scale: %f\n",error_scale);
#else
		  error_scale = eScaler_.ScaleCorrectionUncertainty(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,bit_syst);
#endif
		  ilep.SetPxPyPzE(ilep.Px()*(1.+error_scale), ilep.Py()*(1.+error_scale), ilep.Pz()*(1.+error_scale), ilep.E()*(1.+error_scale)); 
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);   
		}if(ivar==4) { //systematic electron down
		  double error_scale=0.0;
#ifdef YEAR_2017
		  error_scale = ilep.en_enScaleSystDown/ilep.E()-1;
//      printf("Electron stat down error scale: %f\n",error_scale);
#else
		  error_scale = eScaler_.ScaleCorrectionUncertainty(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,bit_syst);
#endif
		  ilep.SetPxPyPzE(ilep.Px()*(1.-error_scale), ilep.Py()*(1.-error_scale), ilep.Pz()*(1.-error_scale), ilep.E()*(1.-error_scale));
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep); 
		}if(ivar==5) { //gain switch electron up
		  double error_scale=0.0;
#ifdef YEAR_2017
		  error_scale = ilep.en_enScaleGainUp/ilep.E()-1;
//      printf("Electron gain up error scale: %f\n",error_scale);
#else
		  error_scale = eScaler_.ScaleCorrectionUncertainty(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,bit_gain);
#endif
		  ilep.SetPxPyPzE(ilep.Px()*(1.+error_scale), ilep.Py()*(1.+error_scale), ilep.Pz()*(1.+error_scale), ilep.E()*(1.+error_scale)); 
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep); 
		}if(ivar==6) { //gain switch electron down
		  double error_scale=0.0;  
#ifdef YEAR_2017
		  error_scale = ilep.en_enScaleGainDown/ilep.E()-1;
//      printf("Electron gain down error scale: %f\n",error_scale);
#else	      
		  error_scale = eScaler_.ScaleCorrectionUncertainty(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,bit_gain);      
#endif
		  ilep.SetPxPyPzE(ilep.Px()*(1.-error_scale), ilep.Py()*(1.-error_scale), ilep.Pz()*(1.-error_scale), ilep.E()*(1.-error_scale));   
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);  
		}if(ivar==7) { //rho resolution Electron up
		  double smearValue = 1.0;
#ifdef YEAR_2017
		  smearValue = ilep.en_enSigmaRhoUp/ilep.E();
//      printf("Electron rho up smear value: %f\n",smearValue);
#else
		  double sigma = eScaler_.getSmearingSigma(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,1,0);
		  TRandom3 *rgen_ = new TRandom3(0);
		  smearValue = rgen_->Gaus(1, sigma) ;
#endif
		  ilep.SetPxPyPzE(ilep.Px()*smearValue, ilep.Py()*smearValue, ilep.Pz()*smearValue, ilep.E()*smearValue);
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);  
		}if(ivar==8) { //rho resolution Electron down
		  double smearValue = 1.0;       
#ifdef YEAR_2017
		  smearValue = ilep.en_enSigmaRhoDown/ilep.E();
//      printf("Electron rho down smear value: %f\n",smearValue);
#else
		  double sigma = eScaler_.getSmearingSigma(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,-1,0);
		  TRandom3 *rgen_ = new TRandom3(0);  
		  smearValue = rgen_->Gaus(1, sigma) ;        
#endif
		  ilep.SetPxPyPzE(ilep.Px()*smearValue, ilep.Py()*smearValue, ilep.Pz()*smearValue, ilep.E()*smearValue);      
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);      
		}if(ivar==9) { //phi resolution Electron down
		  double smearValue = 1.0;
#ifdef YEAR_2017
		  smearValue = ilep.en_enSigmaPhiDown/ilep.E();
//      printf("Electron phi down smear value: %f\n",smearValue);
#else
		  double sigma = eScaler_.getSmearingSigma(phys.run,(fabs(ilep.en_EtaSC)<=1.447),ilep.en_R9, ilep.en_EtaSC, et, ilep.en_gainSeed,0,-1);
		  TRandom3 *rgen_ = new TRandom3(0);  
		  smearValue = rgen_->Gaus(1, sigma);
#endif 
		  ilep.SetPxPyPzE(ilep.Px()*smearValue, ilep.Py()*smearValue, ilep.Pz()*smearValue, ilep.E()*smearValue);        
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);      
		}if(ivar==0){ // nominal
		  if(!runQCD){
		    selLeptonsVar[eleVarNames[ivar]].push_back(ilep);    
		  } else {
		    if( !(ilep.passIdEl) ) selLeptonsVar[eleVarNames[ivar]].push_back(ilep); 
		    //		    if( !(ilep.passIsoEl) ) selLeptonsVar[eleVarNames[ivar]].push_back(ilep);    
		  }
		}
		
	      } if (abs(lepid)==13) { // if mu
		if(!runQCD){
		  selLeptonsVar[eleVarNames[ivar]].push_back(ilep);    
		} else {
		  //		  if( (ilep.mn_relIso>0.4) && (ilep.mn_relIso<4.) && (ilep.mn_trkrelIso>0.4) && (ilep.mn_trkrelIso<4.) ) selLeptonsVar[eleVarNames[ivar]].push_back(ilep);  
		  if( (ilep.mn_relIso>0.15) && (ilep.mn_trkrelIso>0.05) ) selLeptonsVar[eleVarNames[ivar]].push_back(ilep);    
		}
	      }
	    }
	    /*
	    nGoodLeptons++;
	    // std::pair <int,LorentzVector> goodlep;
	    // goodlep = std::make_pair(lepid,ilep);
	    if (abs(lepid)==11) {
	      if (ilep.passIsoEl) goodLeptons.push_back(ilep);
	    } else if (abs(lepid)==13) {
	      if (ilep.passIsoMu) goodLeptons.push_back(ilep);
	    }
	    */
	    
	  } else { // extra loose leptons

	    if (abs(lepid)==11) {
	      hasExtraLepton = (ilep.passIdLooseEl && ilep.passIsoEl && ilep.pt()>10.);
	    } else if (abs(lepid)==13) {
	      hasExtraLepton = ( (ilep.passIdLooseMu && (ilep.passIsoMu) && ilep.pt()>10.) || (ilep.passSoftMuon) );
	    }

	  }

	  if ( hasTightIdandIso && ilep.pt()>20. && ilep.pt()<30.) { 
            vetoLeptons.push_back(ilep); 
          }   

	  if (hasExtraLepton) {
	    nExtraLeptons++;
	    extraLeptons.push_back(ilep);	  }
	} // leptons

	
	//	std::vector<PhysicsObject_Lepton> selLeptons = selLeptonsVar[""];
	PhysicsObjectLeptonCollection selLeptons = selLeptonsVar[""];
	sort(selLeptons.begin(), selLeptons.end(), ptsort());          
	sort(allLeptons.begin(), allLeptons.end(), ptsort()); 

        sort(vetoLeptons.begin(), vetoLeptons.end(), ptsort()); 
        mon.fillHisto("nleptons","raw", selLeptons.size(),weight); 
        mon.fillHisto("ngoodleptons","raw", allLeptons.size(),weight); 
        mon.fillHisto("nleptons","raw_extra", extraLeptons.size(),weight); 

        //Some info on Isolation 
	if(allLeptons.size()>0){ 
          if(abs(allLeptons[0].id)==11) mon.fillHisto("lep_reliso","e_noniso",allLeptons[0].en_relIso,weight); 
          if(abs(allLeptons[0].id)==13) {
	    mon.fillHisto("lep_reliso","mu_noniso",allLeptons[0].mn_relIso,weight); 
	    mon.fillHisto("lep_reliso","mutrk_noniso",allLeptons[0].mn_trkrelIso,weight);    
	  }
	}

	// Lepton thresholds
	if (selLeptons.size()>0) {
	  if(abs(selLeptons[0].id)==11) {
	    if(is2016data || is2016MC) {
	      if (runZH && selLeptons[0].pt()<25.) continue; 
	      if (!runZH && selLeptons[0].pt()<30.) continue;  
	    }
	    if(is2017data || is2017MC) {
	      if (runZH && selLeptons[0].pt()<25.) continue;
	      if (!runZH && selLeptons[0].pt()<35.) continue;
	    }
	    if(is2018data || is2018MC) {
	      if (runZH && selLeptons[0].pt()<25.) continue;
	      if (!runZH && selLeptons[0].pt()<35.) continue;
	    }
	    //	    if(!runZH && selLeptons[0].pt()<30.) continue;
	  }else if(abs(selLeptons[0].id)==13){
	    if(is2016data || is2016MC) {     
	      if (runZH && selLeptons[0].pt()<20.) continue;    
	      if (!runZH && selLeptons[0].pt()<25.) continue; 
	    }
	    if(is2017data || is2017MC) {    
	      if (runZH && selLeptons[0].pt()<20.) continue; 
	      if (!runZH && selLeptons[0].pt()<30.) continue;     
	    }		  
	    if(is2018data || is2018MC) {    
	      if (runZH && selLeptons[0].pt()<20.) continue; 
	      if (!runZH && selLeptons[0].pt()<25.) continue;     
	    }		  
	    //	    if((is2016data || is2016MC) && selLeptons[0].pt()<20.) continue;    
	    // if((is2017data || is2017MC) && selLeptons[0].pt()<25.) continue;
	  }
	}


  // Exactly 1 good lepton
	bool passOneLepton(selLeptons.size()==1);
	bool passDiLepton(selLeptons.size()==2); // && ( abs(selLeptons[0].id)==abs(selLeptons[1].id) ) );
	
	bool passOneLepton_anti(selLeptons.size()>=1);
	if (runQCD) {
	  if (!passOneLepton_anti) continue;
	  // if (goodLeptons.size()==1) continue;
	} else {
	  if (runZH) { 
            if (!passDiLepton) continue; 
            int id1=selLeptons[0].id; 
            int id2=selLeptons[1].id; 
            if(id1*id2>0) continue; // only opposite charge dilepton allow   
	  //	  if (runZH) { 
	  // if (!passDiLepton) continue;
	    //	    LorentzVector zll(selLeptons[0]+selLeptons[1]);
	  }
	  else {if (!passOneLepton) continue; }
	}

       	// lepton ID + ISO scale factors 
	if(isMC && !isQCD) {
	  if (abs(selLeptons[0].id)==11) {
	      // ID + ISO
	    weight *= getSFfrom2DHist(selLeptons[0].en_EtaSC, selLeptons[0].pt(), E_RECO_SF_h );
	    //lepEff.getRecoEfficiency( selLeptons[0].en_EtaSC, 11).first;
	    weight *= getSFfrom2DHist(selLeptons[0].en_EtaSC, selLeptons[0].pt(), E_TIGHTID_SF_h);
	  } else if (abs(selLeptons[0].id)==13) {
	    // ID + ISO
	    musf_id->Fill(selLeptons[0].pt(), fabs(selLeptons[0].eta()), 0.55*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()),MU_ID_SF_h) + 0.45*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_ID_SF_h2 ) );
	    musf_iso->Fill(selLeptons[0].pt(), fabs(selLeptons[0].eta()), 0.55*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()),MU_ISO_SF_h) + 0.45*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_ISO_SF_h2) );
	    musf_trg->Fill(selLeptons[0].pt(), fabs(selLeptons[0].eta()), 0.55*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_TRG_SF_h) + 0.45*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_TRG_SF_h2) );

	    weight *= (getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_ID_SF_h ));
		       //+ 0.45*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_ID_SF_h2) ); 

	    weight *= (getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_ISO_SF_h  ));
	    //		       + 0.45*getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_ISO_SF_h2  ) );       
	  }
	}

	//------------------------------------------------------------------------------------
	// HEM Veto for 2018
	//------------------------------------------------------------------------------------
	jetinHEM = false;
	for(size_t ijet=0; ijet<corrJets.size(); ijet++) {
	  if(corrJets[ijet].pt()>jet_threshold_ && 
	     (corrJets[ijet].eta()>-3.2 && corrJets[ijet].eta()<-1.2) &&
	     (corrJets[ijet].phi()>-1.77 && corrJets[ijet].phi()<-0.67)
	    ) {
	    jetinHEM = true;
	    break;
	  }
	}
	if(is2018data && afterRun319077 && (jetinHEM || eleinHEM) ) continue;
	if(is2018MC && (jetinHEM || eleinHEM)) {
	  weight *= 0.35;
	}
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	if (isMC_ttbar) { //split inclusive TTJets POWHEG sample into tt+bb, tt+cc and tt+light
	//-------------------------------------------------------------------------------------
	  
	  if (verbose) {
	    printf("\n\n");
	    for (auto igen : phys.genparticles) {
	      printf("gen particle id: %d, mom id: %d, mom idx: %d, status: %d\n",
		     igen.id, igen.momid, igen.momidx, igen.status);
	    }
	  }

	  int nHF, nHFc; nHF=nHFc=0;
	    
	  for(size_t ijet=0; ijet<corrJets.size(); ijet++) {
	    if(corrJets[ijet].pt()<jet_threshold_) continue;
	    if(fabs(corrJets[ijet].eta())>2.5) continue;

	    //jet ID
	    if(!corrJets[ijet].isPFLoose) continue;
	    
	     // //check overlaps with selected leptons
	    bool hasOverlap(false);
	    //	    for(size_t ilep=0; ilep<selLeptons.size(); ilep++) {
	    double dR = deltaR( corrJets[ijet], selLeptons[0] ); hasOverlap = (dR<0.4);

	    if(hasOverlap) continue;

	    bool isMatched(false);
  
	    for (auto igen : phys.genparticles) {
	      if(igen.status==62) continue;
	      //only check matching with a b-hadron from the top decay
	      if(fabs(igen.id)!=5) continue; 
	      double dR = deltaR( corrJets[ijet], igen );
	      if (dR<0.4) { isMatched=true; break; }
	    }

	    
	    if(abs(corrJets[ijet].flavid)==5) {
	      if(!isMatched) { nHF++; }
	    } else if (abs(corrJets[ijet].flavid)==4) {
	      nHFc++;// if(!isMatched) { nHFc++; }
	    } 
	    
	  } // end corrJets

	  if(mctruthmode==5) { // only keep events with nHF>0.
	    if (!(nHF>0)) continue;
	  }
	  if(mctruthmode==4) { //only keep events with nHFc>0 and nHF==0.
	    if (!(nHFc>0 && nHF==0)) continue;
	  }
	  if(mctruthmode==1) {
	    if (nHF>0 || (nHFc>0 && nHF==0)) continue;
	  }
	  
	}

	/*
        //split inclusive DY sample into DYToLL and DYToTauTau
        if(isMC && mctruthmode==1113) {
            //if(phys.genleptons.size()!=2) continue;
            if(phys.genleptons.size()==2 && isDYToTauTau(phys.genleptons[0].id, phys.genleptons[1].id) ) continue;
        }
        if(isMC && mctruthmode==15) {
            if(phys.genleptons.size()!=2) continue;
            if(!isDYToTauTau(phys.genleptons[0].id, phys.genleptons[1].id) ) continue;
        }
	*/

	TString tag_cat;
        int evcat(0);
	if (selLeptons.size()==1) evcat = getLeptonId(abs(selLeptons[0].id)); //abs(selLeptons[0].first));
	if (selLeptons.size()>1) {
	  if (runQCD) evcat = getLeptonId(abs(selLeptons[0].id));
	  else evcat =  getDileptonId(abs(selLeptons[0].id),abs(selLeptons[1].id)); 
	}
        switch(evcat) {
        case MUMU :
	  tag_cat="mumu";
	  break;
        case EE   :
	  tag_cat="ee";
	  break;
	case MU  :
	  tag_cat="mu";
	  break;
	case E   :
	  tag_cat="e";
	  break;
        case EMU  :
	  tag_cat="emu"; //continue;
	  break;
	default :
	  continue;
        }

	// 1-lepton
        mon.fillHisto("eventflow",tag_cat,1,weight); 


	if(is2017MC || is2017data){
	    hasEMtrigger = hasEMtrigger || hasEMtrigger2;
	    hasMMtrigger = hasMMtrigger || hasMMtrigger2;
	}
        bool hasTrigger(false);
	if(!isMC) {
	  if(evcat!=fType) continue; 

	  if(evcat==E && !(hasEtrigger)) continue;
	  if(evcat==MU && !(hasMtrigger)) continue;

	  if( is2017data && evcat==EE && !(hasEEtrigger||hasEEtrigger2) ) continue;
	  if(!is2017data && evcat==EE && !hasEEtrigger) continue;
	  //	  if( is2017data && evcat==EE && !((hasEEtrigger||hasEEtrigger2) || hasEtrigger) ) continue;
	  //	  if(!is2017data && evcat==EE && !(hasEEtrigger||hasEtrigger) ) continue;
	  if(evcat==MUMU && !(hasMMtrigger)) continue; //||hasMtrigger) ) continue;
	  if(evcat==EMU && !hasEMtrigger ) continue;

	  //this is a safety veto for the single mu PD
	  if(isSingleMuPD) {
	    if(!hasMtrigger) continue;
	    if(hasMtrigger && hasMMtrigger) continue;
	  }
	  if(isDoubleMuPD) {
	    if(!hasMMtrigger) continue;
	  }

	  //this is a safety veto for the single Ele PD
	  if(isSingleElePD) {
	    if(!hasEtrigger) continue;
	    if( is2017data && hasEtrigger && (hasEEtrigger||hasEEtrigger2) ) continue;
	    if(!is2017data && hasEtrigger && (hasEEtrigger)) continue; //|| hasMtrigger || hasMMtrigger ) ) continue;
	  }
	  if(isDoubleElePD) {
	    //	    if(!hasEEtrigger) continue;  
	    if( is2017data && !(hasEEtrigger || hasEEtrigger2) ) continue;
	    if(!is2017data && !hasEEtrigger) continue;
	    //	    if(hasEEtrigger && (hasMtrigger || hasMMtrigger) ) continue;
	  }
	  /*
	  if(isMuonEGPD) {
	    if(!hasEMtrigger) continue;
	    if(hasEMtrigger && (hasEtrigger || hasEEtrigger || hasMtrigger || hasMMtrigger) ) continue;
	  } 
	  */  
	  /*
	  if(isDoubleMuPD)    { hasTrigger = hasMMtrigger;}
	  if(isSingleMuPD)    { hasTrigger = hasMtrigger  && !hasMMtrigger;}
	  if(isDoubleElePD)   { hasTrigger = hasEEtrigger  && !hasMtrigger  && !hasMMtrigger;}
	  if(isSingleElePD)   { hasTrigger = hasEtrigger  && !hasEEtrigger  && !hasMtrigger && !hasMMtrigger; }
	  if(isMuonEGPD)      { hasTrigger = hasEMtrigger && !hasEtrigger   && !hasEEtrigger && !hasMtrigger && !hasMMtrigger; }
	  */

	  hasTrigger=true;

	} else {
	  if(evcat==E && hasEtrigger ) hasTrigger=true;   
	  if(evcat==MU && hasMtrigger ) hasTrigger=true;   
	  if( is2017MC && evcat==EE && (hasEEtrigger||hasEEtrigger2)) hasTrigger=true; 
	  if(!is2017MC && evcat==EE && hasEEtrigger) hasTrigger=true; 
	  if(evcat==MUMU && hasMMtrigger) hasTrigger=true; 
	  if(evcat==EMU  && hasEMtrigger ) hasTrigger=true;  
	}
	// Apply Trigger requirement:
	if(!hasTrigger) continue;
	

	// TRG Scale Factors
	double myrelIso=-1.; //double mytrkrelIso=-1;

	if (abs(selLeptons[0].id)==11) {
	  // TRG
	  if(is2016MC && !isQCD) {
	    weight*=getSFfrom2DHist(selLeptons[0].pt(), selLeptons[0].en_EtaSC, E_TRG_SF_h1);
	  } else if(is2017MC && !isQCD){//2017 ele TRG scale factor: https://twiki.cern.ch/twiki/bin/viewauth/CMS/Egamma2017DataRecommendations#E/gamma%20Trigger%20Recomendations
	    weight*=0.991;
	  } else if(is2018MC && !isQCD){ // https://twiki.cern.ch/twiki/bin/view/CMS/EgammaRunIIRecommendations
      weight*=1.0;
    }
	    //	    weight *= getSFfrom2DHist(selLeptons[0].pt(), selLeptons[0].en_EtaSC, E_TRG_SF_h1);
	  //weight *= getSFfrom2DHist(selLeptons[0].pt(), selLeptons[0].en_EtaSC, E_TRG_SF_h2);
	  mon.fillHisto("leadlep_pt_raw",tag_cat,selLeptons[0].pt(),weight);   
	  mon.fillHisto("leadlep_eta_raw",tag_cat,selLeptons[0].eta(),weight);     
	  mon.fillHisto("lep_reliso",tag_cat,selLeptons[0].en_relIso,weight);
	  
	  myrelIso=selLeptons[0].en_relIso;
	  
	} else if (abs(selLeptons[0].id)==13) {
	    // TRG
	  if(isMC && !isQCD) {
	    weight *= (getSFfrom2DHist(selLeptons[0].pt(), fabs(selLeptons[0].eta()), MU_TRG_SF_h ));
	  }
	 
	  mon.fillHisto("leadlep_pt_raw",tag_cat,selLeptons[0].pt(),weight);
	  mon.fillHisto("leadlep_eta_raw",tag_cat,selLeptons[0].eta(),weight);
	  mon.fillHisto("lep_reliso",tag_cat,selLeptons[0].mn_relIso,weight);       
	  mon.fillHisto("lep_reliso",tag_cat+"_trk",selLeptons[0].mn_trkrelIso,weight);   

	  myrelIso=selLeptons[0].mn_relIso;
	  //	  mytrkrelIso=selLeptons[0].mn_trkrelIso;
	}
	
	    
	    // Trigger
	mon.fillHisto("eventflow",tag_cat,2,weight);
	
        // pielup reweightiing
        mon.fillHisto("nvtx_raw",   tag_cat, phys.nvtx,      xsecWeight*genWeight);
        mon.fillHisto("nvtxwgt_raw",tag_cat, phys.nvtx,      weight);

	// ---------------------------------------------------------------------------
	
        //
        //JETMET AND BTAGGING ANALYSIS
        //
	if ( verbose ) { printf("\nMissing  pt=%6.1f\n", metP4.pt()); }

	//update the met for lepton energy scales              
	metP4 -= (muDiff - elDiff);
	if ( verbose ) { printf("\nMissing  pt (after lepton energy scale cor.) =%6.1f\n", metP4.pt()); }

	//note this also propagates to all MET uncertainties
	//	METUtils::computeVariation(phys.jets, selLeptons, metP4, variedJets, variedMET, totalJESUnc, ( (is2017data||is2017data) << 0 ) | ( (is2018data||is2018data) << 1));
	// decorrelate JES uncertainties
	//	METUtils::computeVariation(phys.jets, selLeptons, metP4, variedJets, variedMET, totalJESUnc, ( (is2017data||is2017data) << 0 ) | ( (is2018data||is2018data) << 1)); // totalJESUnc -> vector of 27
	METUtils::computeJetVariation(phys.jets, selLeptons,variedJets,totalJESUnc,( (is2017data||is2017data) << 0 ) | ( (is2018data||is2018data) << 1)); // totalJESUnc -> vector of 6

 	//	METUtils::computeVariation(phys.jets, selLeptons, (usemetNoHF ? phys.metNoHF : phys.met), variedJets, variedMET, totalJESUnc, ( (is2017data||is2017data) << 0 ) | ( (is2018data||is2018data) << 1));
	//	for (int isrc = 0; isrc < nsrc; isrc++) {
	//	  delete totalJESUnc[isrc]; 
	//	} 

	//###########################################################
	// The AK8 fat jets configuration
	//###########################################################
	/*
	//AK8 + double-b tagger fat-jet collection
	PhysicsObjectFatJetCollection DBfatJets; // collection of AK8 fat jets

	int ifjet(0);
	for (auto & ijet : fatJets ) {

	  if(ijet.pt()<jet_threshold_) continue;
	  if(fabs(ijet.eta())>2.4) continue;

	  double dR = deltaR( ijet, selLeptons[0] );
	  mon.fillHisto("dRlj_raw","all_fjet",dR,weight);
    
	  if (dR<0.4) continue;
  
	  ifjet++;

	  int count_sbj(0);
	    // Examine soft drop subjets in AK8 jet:
	  count_sbj = ijet.subjets.size(); // count subjets above 20 GeV only
  
	  if ( verbose ) printf("\n\n Print info for subjets in AK8 %3d : ", ifjet);

	  if ( verbose ) {
	    // loop over subjets of AK8 jet
	    for (auto & it : ijet.subjets ) {
	      printf("\n subjet in Ntuple has : pt=%6.1f, eta=%7.3f, phi=%7.3f, mass=%7.3f",   
		     it.pt(),
		     it.eta(),
		     it.Phi(),
		     it.M()
		     );
	    }
	    printf("\n\n");
	  } // verbose
  
	  mon.fillHisto("db_discrim","fjet",ijet.btag0,weight);
	  mon.fillHisto("nsubjets_raw","fjet",count_sbj,weight);
	  mon.fillHisto("sd_mass","fjet",ijet.softdropM,weight);
	  mon.fillHisto("pruned_mass","fjet",ijet.prunedM,weight);

	  if (ijet.softdropM<=40.) {
	    if (ijet.softdropM>=7.)  {
	      mon.fillHisto("db_discrim","fjet_lowm",ijet.btag0,weight);
	    }
	  } else {
	    mon.fillHisto("db_discrim","fjet_highm",ijet.btag0,weight);
	  }

	  bool hasDBtag(ijet.btag0>DBMediumWP);
  
	  if (ijet.softdropM>=7. && ijet.softdropM<=40. && count_sbj>0) {
	    if (hasDBtag) DBfatJets.push_back(ijet);
	  }
	  
	} // AK8 fatJets loop

	//--------------------------------------------------------------------------
	// AK8 + double-b jets
	sort(DBfatJets.begin(), DBfatJets.end(), ptsort());

	mon.fillHisto("nbjets_raw","nfatJet", DBfatJets.size(),weight);

  	int is(0);
	for (auto & jet : DBfatJets) {
	   mon.fillHisto("jet_pt_raw", "fat"+htag[is], jet.pt(),weight);
	   mon.fillHisto("jet_eta_raw", "fat"+htag[is], jet.eta(),weight);
	   is++;
	   if (is>3) break; // plot only up to 4 b-jets ?
	}
	*/
	//--------------------------------------------------------------------------

	//###########################################################
	// Soft b-jets from SVs configuration
	//###########################################################
	
	// SVs collection
	//PhysicsObjectSVCollection SVs;
	PhysicsObjectSVCollection SVs_raw; // non-cross-cleaned secondary vertices
	  
	for (auto & isv : secVs) {
	  
	  if (isv.pt()>=jet_threshold_) continue; // SV pT>20 GeV
	    
	  mon.fillHisto("softb_ntrk","raw",isv.ntrk,weight);
	  if (isv.ntrk<3) continue; // nTrks associated to SV >= 3
	  
	  if ( verbose ) {
	    
	    printf("\n SV has : pt=%6.1f, ntrk=%3d, dxy=%7.3f, dxyz_signif=%7.3f, isv.cos_dxyz_p=%7.3f",
		   isv.pt(),   
		   isv.ntrk,
		   isv.dxy,
		   isv.dxyz_signif,
		   isv.cos_dxyz_p
		   );
	    
	  } // verbose 
	  
	    // check overlap with any other jet
	  // bool hasOverlap(false);
	  mon.fillHisto("softb_dxy","raw",isv.dxy,weight);
	  //	  if (isv.sv_mc_mcbh_ind>0)mon.fillHisto("softb_dxy","raw_true",isv.dxy,weight);
	  mon.fillHisto("softb_dxyz_signif","raw",isv.dxyz_signif,weight);
	  //	  if (isv.sv_mc_mcbh_ind>0) mon.fillHisto("softb_dxyz_signif","raw_true",isv.dxyz_signif,weight);
	  mon.fillHisto("softb_cos","raw",isv.cos_dxyz_p,weight);
	  //	  if (isv.sv_mc_mcbh_ind>0) mon.fillHisto("softb_cos","raw_true",isv.cos_dxyz_p,weight);

	  if (isv.dxy>3.) continue;
	  if (isv.dxyz_signif<4.) continue;
	  if (isv.cos_dxyz_p<0.98) continue;
	  
	  SVs_raw.push_back(isv);
	  
	}
	    
	// 	sort(SVs.begin(), SVs.end(), ptsort());
	sort(SVs_raw.begin(), SVs_raw.end(), ptsort());

	
	//##############################################################################
        //### 	// LOOP ON SYSTEMATIC VARIATION FOR THE STATISTICAL ANALYSIS
	//##############################################################################
	
	double PDFalphaSWeight(0.);

	if (runSystematics) {
	  // for POWHEG, MADGRAPH based samples
	  // retrive PDFweights from ntuple
	  TH1F *pdf_h = new TH1F();

	  for(int npdf=0; npdf<ev.npdfs; npdf++) {
	    pdf_h->Fill(ev.pdfWeights[npdf]);
	  }

	  double pdfError(0.);
	  if (ev.npdfs>0 && pdf_h!=NULL) pdfError = pdf_h->GetRMS();
	  //	  else cout << "pdfError: " << pdfError << endl; 

	  delete pdf_h;

	  // retrive alphaSweights from ntuple
	  TH1F *alphaS_h = new TH1F();

	  for(int nalpha=0; nalpha<ev.nalphaS; nalpha++) {
	    alphaS_h->Fill(ev.alphaSWeights[nalpha]);
	  }

	  double alphaSError(0.);
	  if(ev.nalphaS>0 && alphaS_h!=NULL) alphaSError= alphaS_h->GetRMS();

	  delete alphaS_h;
	  //      cout << "alphaSError: " << alphaSError << endl;

	  PDFalphaSWeight = sqrt(pdfError*pdfError + alphaSError*alphaSError);
	}

	float iweight = weight;   //nominal

	for(size_t ivar=0; ivar<nvarsToInclude; ivar++){
	  if(!isMC && ivar>0 ) continue; //loop on variation only for MC samples
	  if(isQCD && ivar>0) continue; // skip systematics from MC QCD since data-driven will follow

	  if ( verbose ) { std::cout << "\n\n Running variation: " << varNames[ivar] << " with ivar == " << ivar << std::endl; }
	  
	  std::vector<TString> tags(1,"all");
	  
	  weight = iweight; // reset to nominal weight

	  LorentzVector imet(metP4); // = variedMET[0];
	  if(varNames[ivar]=="_jerup" ) imet = variedMET[3]; 
	  if(varNames[ivar]=="_jerdown") imet = variedMET[4];
	  if(string(varNames[ivar].Data()).find("_jesup") != string::npos) imet = variedMET[1];
	  if(string(varNames[ivar].Data()).find("_jesdown") != string::npos) imet = variedMET[2];
	  if(varNames[ivar]=="_umetup") imet = variedMET[5];
	  if(varNames[ivar]=="_umetdown") imet = variedMET[6];

	  if(varNames[ivar]=="_lesup") {// || varNames[ivar]=="_lesdown") { 
	    if (evcat==E || evcat==EE) imet = variedMET[9];
	    else if (evcat==MU || evcat==MUMU) imet = variedMET[7];
	  } else if(varNames[ivar]=="_lesdown") {      
	    if (evcat==E || evcat==EE) imet = variedMET[10];
	    else if (evcat==MU || evcat==MUMU) imet = variedMET[8];
	  }
	  
	  PhysicsObjectJetCollection vJets = variedJets[0];
	  if(varNames[ivar]=="_jerup" || varNames[ivar]=="_jerdown" || (string(varNames[ivar].Data()).find("_jesup") != string::npos) || (string(varNames[ivar].Data()).find("_jesdown") != string::npos) ){
	    vJets = variedJets[ivar];
	  } 
	  
            //pileup
	  if(varNames[ivar]=="_puup") weight *= ( TotalWeight_plus/puWeight ); //pu up
	  if(varNames[ivar]=="_pudown") weight *= ( TotalWeight_minus/puWeight ); //pu down

	 
	  if(varNames[ivar]=="_pdfup")    weight *= (1.+PDFalphaSWeight);
	  else if(varNames[ivar]=="_pdfdown") weight *= (1.-PDFalphaSWeight);

	  if(varNames[ivar]=="_stat_eup" ||  varNames[ivar]=="_sys_eup" ||  varNames[ivar]=="_GS_eup" ||  varNames[ivar]=="_resRho_eup") { imet = variedMET[9]; }
	  if(varNames[ivar]=="_stat_edown" ||    varNames[ivar]=="_sys_edown" || varNames[ivar]=="_GS_edown" || varNames[ivar]=="_resRho_edown" || varNames[ivar]=="_resPhi_edown")  { imet = variedMET[10]; }


	  //	  if(varNames[ivar]=="_scale_mup" || varNames[ivar]=="_scale_mdown")
	  if (varNames[ivar]=="_stat_eup" || varNames[ivar]=="_stat_edown" ||
	      varNames[ivar]=="_sys_eup" || varNames[ivar]=="_sys_edown" || varNames[ivar]=="_GS_eup" || varNames[ivar]=="_GS_edown" ||
	      varNames[ivar]=="_resRho_eup" || varNames[ivar]=="_resRho_edown" || varNames[ivar]=="_resPhi_edown")  {

	    //	    imet = variedMET[];
	    
	    if (evcat==E || evcat==EE) {
	      selLeptons = selLeptonsVar[varNames[ivar].Data()];
	    } else { continue; } //selLeptonsVar[varNames[0].Data()]; }
	  } else {
	    selLeptons = selLeptonsVar[varNames[0].Data()]; 
	  }
	  
	  if ( verbose ) {
	    printf("\nMissing  pt=%6.1f\n", imet.pt());
	    int ilep(0);
	    for (auto & i : selLeptons) {
	      printf("selLepton is %s, and has : pt=%6.1f, eta=%7.3f, phi=%7.3f, mass=%7.3f\n",   
		     (abs(selLeptons[ilep].id)==11 ? "ELE" : "MUON") ,
		     selLeptons[ilep].pt(),
		     selLeptons[ilep].eta(),
		     selLeptons[ilep].Phi(),
		     selLeptons[ilep].M()
		     );
	      ilep++;
	    }
	  } // verbose

	  
        //###########################################################
        // AK4 jets ,
        // AK4 jets + CSVloose b-tagged configuration
        //###########################################################

	  PhysicsObjectJetCollection GoodIdJets;
	  PhysicsObjectJetCollection CSVLoosebJets; // used to define the SRs

	  int nJetsGood30(0);
	  int nCSVLtags(0),nCSVMtags(0),nCSVTtags(0);
	  double BTagWeights(1.0);
	  
	  float mindphijmet(999.);
	  for(size_t ijet=0; ijet<vJets.size(); ijet++) {
	    
	    if(vJets[ijet].pt()<jet_threshold_) continue;
	    if(fabs(vJets[ijet].eta())>2.5) continue;
	    
	    //jet ID
	    if(!vJets[ijet].isPFLoose) continue;
	    //if(vJets[ijet].pumva<0.5) continue;
	    
	    // //check overlaps with selected leptons
	    bool hasOverlap(false);
	    //	    for(size_t ilep=0; ilep<selLeptons.size(); ilep++) {
	    double dR = deltaR( vJets[ijet], selLeptons[0] ); hasOverlap = (dR<0.4); 
	    if (ivar==0) mon.fillHisto("dRlj_raw",tag_cat,dR,weight); 

	    if(hasOverlap) continue;
	    
	    GoodIdJets.push_back(vJets[ijet]);
	    if(vJets[ijet].pt()>30) nJetsGood30++;
	    
	    // Dphi (j,met)
	    float dphijmet=fabs(deltaPhi(vJets[ijet].phi(),imet.phi()));
	    if (dphijmet<mindphijmet) mindphijmet=dphijmet;

	    
	    if(vJets[ijet].pt()>20. && fabs(vJets[ijet].eta())<2.4) {
	      // B-tagging
	      bool hasCSVtagL,hasCSVtagM;
	      double btag_dsc = -1;
	      if ( use_DeepCSV ) {btag_dsc = vJets[ijet].btag1;} else {btag_dsc = vJets[ijet].btag0;}
	      if (ivar==0) {
		mon.fillHisto("b_discrim",b_tagging_name,btag_dsc,weight);
		//		if (vJets[ijet].motherid == 36) mon.fillHisto("b_discrim",b_tagging_name+"_true",btag_dsc,weight);
	      }
	      hasCSVtagL = btag_dsc>LooseWP; hasCSVtagM = btag_dsc>MediumWP;
	      bool hasCSVtagLUp = hasCSVtagL, hasCSVtagMUp = hasCSVtagM;
	      bool hasCSVtagLDown = hasCSVtagL, hasCSVtagMDown = hasCSVtagM;
	      
	      //https://twiki.cern.ch/twiki/bin/view/CMS/BTagShapeCalibration
	      //Using .root histogram files
	      //Quick guide to iSys number and systematic to which it represents:
	      //iSys       :      7,	    8,			   9,			   10,			  11,			   12,		 13,		 14
	      //Systematics: JES Up, JES Down, LF (contamination) Up, LF (contamination) Down, HF (contamination) Up, HF (contamination) Down, HF Stats1 Up, HF Stats1 Down
	      //iSys       :	       15,	       16,	     17,	     18,	   19,		   20,		  21,		   22,		  23,		   24
	      //Systematics: HF Stats2 Up, HF Stats2 Down, LF Stats1 Up, LF Stats1 Down, LF Stats2 Up, LF Stats2 Down, Charm Err1 Up, Charm Err1 Down, Charm Err2 Up, Charm Err2 Down
	      if(nMethod == 2 && isMC) {
	        double wgt_csv_hf, wgt_csv_lf, wgt_csv_cf;
		int iSys=0;
		//determining the iSys number:
		if(abs(vJets[ijet].flavid)==5){
		  if (varNames[ivar]=="_btagup") iSys = 11;//HF (contamination) Up
		  else if(varNames[ivar]=="_btagdown") iSys = 12;//HF (contamination) Down
		  else iSys = 0; //nominal
		} else if(abs(vJets[ijet].flavid)==4) {
		  if (varNames[ivar]=="_ctagup") iSys = 11;//HF (contamination) Up
		  else if (varNames[ivar]=="_ctagdown")	iSys = 12;//HF (contamination) Down
		  else  iSys = 0; //nominal
		}else {
		  if (varNames[ivar]=="_ltagup") iSys = 9;//LF (contamination) Up
		  else if (varNames[ivar]=="_ltagdown") iSys = 10;//LF (contamination) Down
		  else iSys = 0; //nominal
		}
		double wgt_csv = get_csv_wgt(vJets[ijet].pt(), vJets[ijet].eta(), btag_dsc, vJets[ijet].flavid, iSys, wgt_csv_hf, wgt_csv_lf, wgt_csv_cf);
		//printf("Btagging SF: %f\n", wgt_csv);
		weight *= wgt_csv;
	      } //end isMC

	      
	      if (nMethod == 1 && isMC) { 
		//https://twiki.cern.ch/twiki/bin/viewauth/CMS/BtagRecommendation80X
		btsfutil.SetSeed(ev.event*10 + ijet*10000);// + ivar*10);
		float bSFLoose, bSFMedium;	
		if(abs(vJets[ijet].flavid)==5) {
		  if(use_DeepCSV) {
		    //        beff=btsfutil.getBTagEff(vJets[ijet].pt(),"bLOOSE");
		    //if(ivar==0)mon.fillHisto("btagEff_b","default",btsfutil.getBTagEff(vJets[ijet].pt(),"bLOOSE"),1.0);
		    beffLoose=getSFfrom2DHist(vJets[ijet].pt(), fabs(vJets[ijet].eta()), btagEffLoose_b); 
		    beffMedium=getSFfrom2DHist(vJets[ijet].pt(), fabs(vJets[ijet].eta()), btagEffMedium_b);
		    //if(ivar==0)mon.fillHisto("btagEff_b","new",beff, 1.0);
		  }
		  //  80X recommendation
		  if (varNames[ivar]=="_btagup") {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("up", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagLUp  , btagReaderLoose.eval_auto_bounds("up", BTagEntry::FLAV_B ,
		    //									  vJets[ijet].eta(), vJets[ijet].pt()), beff); hasCSVtagL=hasCSVtagLUp;
		    btsfutil.applySF2WPs(hasCSVtagLUp, hasCSVtagMUp, bSFLoose, bSFMedium, beffLoose, beffMedium);
		    hasCSVtagL=hasCSVtagLUp; hasCSVtagM=hasCSVtagMUp;
		  } else if ( varNames[ivar]=="_btagdown") {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("down", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagLDown, btagReaderLoose.eval_auto_bounds("down", BTagEntry::FLAV_B ,
		    //									  vJets[ijet].eta(), vJets[ijet].pt()), beff); hasCSVtagL=hasCSVtagLDown;
		    btsfutil.applySF2WPs(hasCSVtagLDown, hasCSVtagMDown, bSFLoose, bSFMedium, beffLoose, beffMedium);
		    hasCSVtagL=hasCSVtagLDown; hasCSVtagM=hasCSVtagMDown;
		  } else {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("central", BTagEntry::FLAV_B,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagL , btagReaderLoose.eval_auto_bounds("central", BTagEntry::FLAV_B ,
		    //								       vJets[ijet].eta(), vJets[ijet].pt()), beff); 
		    btsfutil.applySF2WPs(hasCSVtagL, hasCSVtagM, bSFLoose, bSFMedium, beffLoose, beffMedium);
		
		  }
		} else if(abs(vJets[ijet].flavid)==4) {
		  if(use_DeepCSV){ 
		    //        beff=btsfutil.getBTagEff(vJets[ijet].pt(),"cLOOSE");
		    //if(ivar==0)mon.fillHisto("btagEff_c","default",btsfutil.getBTagEff(vJets[ijet].pt(),"cLOOSE"),1.0);
		    beffLoose=getSFfrom2DHist(vJets[ijet].pt(), fabs(vJets[ijet].eta()), btagEffLoose_c); 
		    beffMedium=getSFfrom2DHist(vJets[ijet].pt(), fabs(vJets[ijet].eta()), btagEffMedium_c); 
		    //if(ivar==0)mon.fillHisto("btagEff_c","new",beff, 1.0);
		  }
		  //  80X recommendation
		  if (varNames[ivar]=="_ctagup") {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("up", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagLUp  , btagReaderLoose.eval_auto_bounds("up", BTagEntry::FLAV_C , 
		    //									  vJets[ijet].eta(), vJets[ijet].pt()), beff);hasCSVtagL=hasCSVtagLUp;
		    btsfutil.applySF2WPs(hasCSVtagLUp, hasCSVtagMUp, bSFLoose, bSFMedium, beffLoose, beffMedium);
		    hasCSVtagL=hasCSVtagLUp; hasCSVtagM=hasCSVtagMUp;
		  } else if ( varNames[ivar]=="_ctagdown") {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("down", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagLDown, btagReaderLoose.eval_auto_bounds("down", BTagEntry::FLAV_C , 
		    //									  vJets[ijet].eta(), vJets[ijet].pt()), beff); hasCSVtagL=hasCSVtagLDown;
		    btsfutil.applySF2WPs(hasCSVtagLDown, hasCSVtagMDown, bSFLoose, bSFMedium, beffLoose, beffMedium);
		    hasCSVtagL=hasCSVtagLDown; hasCSVtagM=hasCSVtagMDown;
		  } else {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("central", BTagEntry::FLAV_C,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagL , btagReaderLoose.eval_auto_bounds("central", BTagEntry::FLAV_C ,
		    //								       vJets[ijet].eta(), vJets[ijet].pt()), beff);
		    btsfutil.applySF2WPs(hasCSVtagL, hasCSVtagM, bSFLoose, bSFMedium, beffLoose, beffMedium);
		  }
		} else {
		  if(use_DeepCSV){
		    //        leff=btsfutil.getBTagEff(vJets[ijet].pt(),"lLOOSE");
		    //if(ivar==0)mon.fillHisto("btagEff_udsg","default",btsfutil.getBTagEff(vJets[ijet].pt(),"lLOOSE"),1.0);
		    leffLoose=getSFfrom2DHist(vJets[ijet].pt(), fabs(vJets[ijet].eta()), btagEffLoose_udsg);
		    leffMedium=getSFfrom2DHist(vJets[ijet].pt(), fabs(vJets[ijet].eta()), btagEffMedium_udsg);
		    //if(ivar==0)mon.fillHisto("btagEff_udsg","new",leff, 1.0); 
		  }
		  //  80X recommendation
		  if (varNames[ivar]=="_ltagup") {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		   }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("up", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagLUp  , btagReaderLoose.eval_auto_bounds("up", BTagEntry::FLAV_UDSG   , 
		    // 								      vJets[ijet].eta(), vJets[ijet].pt()), leff);hasCSVtagL=hasCSVtagLUp;
		    btsfutil.applySF2WPs(hasCSVtagLUp, hasCSVtagMUp, bSFLoose, bSFMedium, leffLoose, leffMedium);
		    hasCSVtagL=hasCSVtagLUp; hasCSVtagM=hasCSVtagMUp;
		  } else if ( varNames[ivar]=="_ltagdown") {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{  
		      bSFLoose = btagReaderLoose.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("down", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagLDown, btagReaderLoose.eval_auto_bounds("down", BTagEntry::FLAV_UDSG   , 
		    //								      vJets[ijet].eta(), vJets[ijet].pt()), leff);hasCSVtagL=hasCSVtagLDown;
		    btsfutil.applySF2WPs(hasCSVtagLDown, hasCSVtagMDown, bSFLoose, bSFMedium, leffLoose, leffMedium);
		    hasCSVtagL=hasCSVtagLDown; hasCSVtagM=hasCSVtagMDown;
		  } else {
		    if(is2017MC){
		      bSFLoose = btagReaderLoose1.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				 btagReaderLoose2.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				 btagReaderLoose3.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		      bSFMedium = btagReaderMedium1.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi1 + 
				  btagReaderMedium2.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi2 +
				  btagReaderMedium3.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt())*bTag_lumi3;
		     bSFLoose /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		     bSFMedium /= (bTag_lumi1 + bTag_lumi2 + bTag_lumi3);
		    }else{
		      bSFLoose = btagReaderLoose.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt());
		      bSFMedium = btagReaderMedium.eval_auto_bounds("central", BTagEntry::FLAV_UDSG,vJets[ijet].eta(), vJets[ijet].pt());
		    }
		    //btsfutil.modifyBTagsWithSF(hasCSVtagL , btagReaderLoose.eval_auto_bounds("central", BTagEntry::FLAV_UDSG ,
		    //								       vJets[ijet].eta(), vJets[ijet].pt()), leff);
		    btsfutil.applySF2WPs(hasCSVtagL, hasCSVtagM, bSFLoose, bSFMedium, leffLoose, leffMedium);
		  }
		}
	      } // isMC
	      

	      if ( verbose) {
		printf("AK4-Jet has : pt=%6.1f, eta=%7.3f, phi=%7.3f, mass=%7.3f\n",     
		       vJets[ijet].pt(),    
		       vJets[ijet].eta(),  
		       vJets[ijet].phi(),
		       vJets[ijet].M()  
		       );

		if(hasCSVtagL) {
		  printf("B-Jet has : pt=%6.1f, eta=%7.3f, phi=%7.3f, mass=%7.3f\n",   
			 vJets[ijet].pt(),
			 vJets[ijet].eta(),
			 vJets[ijet].phi(),
			 vJets[ijet].M()
			 );
		}
	      } // verbose
	      
	      nCSVLtags += hasCSVtagL;
	      nCSVMtags += hasCSVtagM;
	      nCSVTtags += (btag_dsc>TightWP);
	      
	      // Fill b-jet vector:
	      //if (hasCSVtagL) {  CSVLoosebJets.push_back(vJets[ijet]); }
	      if (hasCSVtagM) {  CSVLoosebJets.push_back(vJets[ijet]); }
	    } // b-jet loop
	    
	  } // jet loop


	  //--------------------------------------------------------------------------
	  // 	  // AK4 jets:
	  //--------------------------------------------------------------------------
	  sort(GoodIdJets.begin(), GoodIdJets.end(), ptsort());

	  //--------------------------------------------------------------------------
	  // Secondary vertices (soft-b collection)
	  //--------------------------------------------------------------------------
	  sort(CSVLoosebJets.begin(), CSVLoosebJets.end(), ptsort());

	  // SVs cross-cleaned with AK4 + CSV jets
	  PhysicsObjectSVCollection SVs;
	  
	  int isoft(0);
	  for (auto & isv : SVs_raw) {
	    // check overlap with any other jet
	    bool hasOverlap(false);

	     // plot minDR(SV,b)
	    float dRmin_csv(999.);
	    // for (auto & it : CSVLoosebJets) {
	    for (auto & it : GoodIdJets) {
	      double dR=deltaR(it, isv);
	      if (dR<dRmin_csv) dRmin_csv=dR;
	    }
	    if(ivar==0) {mon.fillHisto("dR_raw","sv_b",dRmin_csv,weight); }
	    
	    //if (!runDBversion) { // use soft-b tags only if AK8 jets are not used
	    hasOverlap=(dRmin_csv<0.4);
	    if (!hasOverlap) {// continue;
	      // Fill final soft-bs from SVs
	      SVs.push_back(isv);  isoft++;      
	    }
	    if (isoft>=1) break; //only allow [0,1] SVs to enter    
	  }

	  // JET KINEMATICS
	  if(ivar==0){

	    // Jet kinematics
	    mon.fillHisto("njets_raw",tag_cat+"_"+"nj", GoodIdJets.size(),weight);
	    
	    // AK4 jets pt:
	    int is(0);     //	    is=0;
	    for (auto & jet : GoodIdJets) {
	      mon.fillHisto("jet_pt_raw", tag_cat+"_"+"jet"+htag[is], jet.pt(),weight); 
	      mon.fillHisto("jet_eta_raw", tag_cat+"_"+"jet"+htag[is], jet.eta(),weight); 
	      mon.fillHisto("jet_phi_raw",tag_cat+"_"+"jet"+htag[is], jet.phi(),weight); 
	      
	      if (use_DeepCSV) { mon.fillHisto("b_discrim",tag_cat+"_"+b_tagging_name+htag[is],jet.btag1,weight); }
	      else { mon.fillHisto("b_discrim",tag_cat+"_"+b_tagging_name+htag[is],jet.btag0,weight); }
	      
	      is++; 
	      if (is>3) break;
	    }
	    
	    
	    //-------------------------------------------------------------------
	    // AK4 + DeepCSV jets
	    //-------------------------------------------------------------------
	    mon.fillHisto("nbjets_raw",tag_cat+"_"+"nb", CSVLoosebJets.size(),weight);
	    mon.fillHisto("nbjets_raw",tag_cat+"_"+"nb_LOOSE", nCSVLtags, weight);
	    mon.fillHisto("nbjets_raw",tag_cat+"_"+"nb_MEDIUM", nCSVMtags, weight);
	    mon.fillHisto("nbjets_raw",tag_cat+"_"+"nb_TIGHT", nCSVTtags, weight);
	    
	    is=0; 
	    for (auto & jet : CSVLoosebJets) {
	      mon.fillHisto("jet_pt_raw", tag_cat+"_"+b_tagging_name+htag[is], jet.pt(),weight); 
	      mon.fillHisto("jet_eta_raw", tag_cat+"_"+b_tagging_name+htag[is], jet.eta(),weight); 
	      mon.fillHisto("jet_phi_raw", tag_cat+"_"+b_tagging_name+htag[is], jet.phi(),weight);
	      
	      is++;
	      if (is>3) break; // plot only up to 4 b-jets ?
	    }
	    
	    //--------------------------------------------------------------------------
	    // Soft-b tagging properties
	    //--------------------------------------------------------------------------
	    mon.fillHisto("nbtags_raw",tag_cat,SVs.size(),weight);
	    
	    is=0;
	    for (auto & isv : SVs) {
	      mon.fillHisto("softjet_pt_raw", tag_cat+"_"+"softb"+htag[is], isv.pt(),weight);
	      mon.fillHisto("jet_eta_raw", tag_cat+"_"+"softb"+htag[is], isv.eta(),weight);
	      is++;
	      if (is>3) break;
	    }
	    
	    // DR between 2 SVs
	    if (SVs.size()>1) {
	      double dR=deltaR(SVs[0], SVs[1]);
	      mon.fillHisto("dR_raw",tag_cat+"_"+"svs",dR,weight);
	    }
	    
	    // dphi(jet,MET)
	    mon.fillHisto("dphijmet",tag_cat+"_"+"raw",mindphijmet,weight);
	  } //ivar=0
	  
	  
	  
	  //#########################################################
	  //####  RUN PRESELECTION AND CONTROL REGION PLOTS  ########
	  //#########################################################
	  
	  bool passZmass(true);          

	  LorentzVector wsum=imet+selLeptons[0];
	  // mtW
	  double tMass = 2.*selLeptons[0].pt()*imet.pt()*(1.-TMath::Cos(deltaPhi(selLeptons[0].phi(),imet.phi())));

	  if (runZH){
	    LorentzVector zll(selLeptons[0]+selLeptons[1]);    
	    //passZmass=(fabs(zll.mass()-91)<15);        
	    passZmass=(zll.mass()<100 && zll.mass()>85);        

	    wsum+=selLeptons[1];

	    double dphi=fabs(deltaPhi(imet.phi(),zll.phi()));
	    tMass=2*imet.pt()*zll.pt()*(1-cos(dphi));
	  }

	  if(ivar==0) {
	    mon.fillHisto("pfmet","raw"+tag_cat,imet.pt(),weight);
	    mon.fillHisto("mtw","raw"+tag_cat,sqrt(tMass),weight);
	    mon.fillHisto("ptw","raw"+tag_cat,wsum.pt(),weight);
	  }
	  
	  //-------------------------------------------------------------------

	  //MET>25 GeV 
	  bool passMet25(imet.pt()>25);
	  if(runZH) passMet25=true;

	  //-------------------------------------------------------------------
	  //mtW >50 GeV
	  bool passMt(sqrt(tMass)>50.); // && sqrt(tMass)<250.);
	  if(runZH) passMt=true;

	  // Z-mass window (only effective in ZH) 
	  if(!passZmass) continue;     
 
	  //-------------------------------------------------------------------
	  // First set all b-tags (x-cleaned) in one vector<LorentzVector>
	  vector<LorentzVector> GoodIdbJets;
	  for (auto & i : CSVLoosebJets) { GoodIdbJets.push_back(i);}
	  for (auto & i : SVs) {  GoodIdbJets.push_back(i); }

	  vector<LorentzVector> pseudoGoodIdbJets;
	  for (auto & i : GoodIdJets) { pseudoGoodIdbJets.push_back(i);}
	  for (auto & i : SVs) {  pseudoGoodIdbJets.push_back(i); } // have you x-cleaned jets and SVs?
	  
	  //At least 2 jets && 2 b-tags  
	  //	  bool passNJ2(GoodIdJets.size()>=3 && GoodIdbJets.size()>=2);     
	  // opposed to ATLAS with Nj>2, as here we add soft b-tags      

	  //At least 1 MediumWP b-tag --> NEED to RECONSIDER       
	  bool passMnBTag(true);       
	  if (CSVLoosebJets.size()>0 && nCSVMtags<=0) passMnBTag=false;

	  //At least 2 jets && 2 b-tags   
	  bool passNJ2(GoodIdJets.size()>=3 && GoodIdbJets.size()>=2 && passMnBTag);

	  // N-1 Plots
	  if (ivar==0) {
	    if (passMet25) {
	      mon.fillHisto("eventflow",tag_cat,3,weight);
	      
	      // MT CUT
	      if (passMt) {
		mon.fillHisto("eventflow",tag_cat,4,weight);
		
		// Lepton kinematics
		mon.fillHisto("leadlep_pt_raw",tag_cat+"_metmt",selLeptons[0].pt(),weight);
		mon.fillHisto("leadlep_eta_raw",tag_cat+"_metmt",selLeptons[0].eta(),weight);
		
		// NJET CUT
		if (passNJ2) {
		  mon.fillHisto("eventflow",tag_cat,5,weight);
		  
		  // Lepton kinematics
		  mon.fillHisto("leadlep_pt_raw",tag_cat+"_nj2",selLeptons[0].pt(),weight);
		  mon.fillHisto("leadlep_eta_raw",tag_cat+"_nj2",selLeptons[0].eta(),weight);
		}
		
		
	      }
	    }
	  }
	  
	  if(!passNJ2) continue;
	  //	  if (!passMnBTag) continue; //at least 1 MediumWP b-tag if nBjets>0
	  
	  //#########################################################
	  //####  RUN PRESELECTION AND CONTROL REGION PLOTS  ########
	  //#########################################################

	  // Event categorization based on (n-btag, n-jet) after leptonic selection
	  int evtCatPlot = eventCategoryPlot.Get(phys,&GoodIdbJets, &pseudoGoodIdbJets);
	  //	  if (evtCatPlot<3) continue;

	  // soft b-tag SF and uncertainty: 1.05 +/- 0.16
	  if (isMC) {
	    for (auto & i : SVs) {  
	      weight *= 1.05;
	      if (varNames[ivar]=="_softbup") { weight *= 1.21; }
	      if (varNames[ivar]=="_softbdown") { weight *= 0.89; }
	      //else { weight *= 1.05; }
	    }
	  }
	  
	  if (ivar==0) mon.fillHisto("evt_cat",tag_cat+"_"+"sel1", evtCatPlot,weight);
	  
	  // //Define ABCD regions
	  TString tag_qcd; tag_qcd="";
	  
	  if (passMet25 && passMt) {
	    if (ivar==0) mon.fillHisto("evt_cat",tag_cat+"_"+"sel2", evtCatPlot,weight);
	    
	    if (!runQCD) {tag_qcd="_A_";} // region A
	    else {tag_qcd="_B_";} // region B
	  } else if (!passMet25 && sqrt(tMass)<50.) {
	    if(runZH) continue;
	    if (!runQCD) {tag_qcd="_C_";} // region C
	    else {tag_qcd="_D_";} // region D
	  } else { continue; }
	 
	  if(ivar==0 && reweightDYZPt && isMC_DY ){
	    mon.fillHisto("jetsMulti","alljets",GoodIdJets.size(),1);
	    mon.fillHisto("ptw","alljets",zpt,weight);
	    if(GoodIdJets.size()==3) {mon.fillHisto("ptw","3jets",zpt,weight);}
	    else if(GoodIdJets.size()==4) {mon.fillHisto("ptw","4jets",zpt,weight);}
	    else if(GoodIdJets.size()>=5) {mon.fillHisto("ptw","5+jets",zpt,weight);}

	    TString event_cat = eventCategoryPlot.GetLabel(evtCatPlot);
	    mon.fillHisto("ptw",event_cat+"_jets",zpt,weight);
	  }

	  if(isMC_DY && !dtag.Contains("amcNLO") && !reweightDYZPt){
	    if(GoodIdJets.size()==3) {weight *= getSFfrom1DHist(zpt, zptSF_3j);}// std::cout << "3j: " << zpt << ", sf: " << getSFfrom1DHist(zpt, zptSF_3j) << std::endl;}
	    else if(GoodIdJets.size()==4) {weight *= getSFfrom1DHist(zpt, zptSF_4j);}// std::cout << "4j: " << zpt << ", sf: " << getSFfrom1DHist(zpt, zptSF_4j) << std::endl;}
	    else if(GoodIdJets.size()>=5) {weight *= getSFfrom1DHist(zpt, zptSF_5j);}// std::cout << "5j: " << zpt << ", sf: " << getSFfrom1DHist(zpt, zptSF_5j) << std::endl;}
	  }

	  
	  //Define event category according to Nb multiplicity: Nb=0->W CR, Nb=1,2->top CR, Nb=3,4->SR
	  //event category
	  int eventSubCat(-1);
	  if (runZH) eventSubCat = eventCategoryInst_Zh.Get(phys,&GoodIdbJets, &pseudoGoodIdbJets);
	  else eventSubCat = eventCategoryInst_Wh.Get(phys,&GoodIdbJets, &pseudoGoodIdbJets);  
	  // remove events other than the Signal and Control regions.
	  if (eventSubCat<0) continue; 
	  
	  TString tag_subcat;
	  if (runZH) tag_subcat = eventCategoryInst_Zh.GetLabel(eventSubCat);
	  else tag_subcat = eventCategoryInst_Wh.GetLabel(eventSubCat);  
	  tags.push_back(tag_cat+tag_qcd+tag_subcat); // add jet binning category
	  //	  if (fabs(mindphijmet)>0.5) tags.push_back(tag_cat+tag_qcd+"passDPHI_"+tag_subcat);
	  
	  bool isSignalRegion(false);
	  if (tag_subcat.Contains("CR") || evcat==EMU) { // ||  tag_subcat.Contains("CR_nonTT")) {
	    // contains (2b,3j) and (2b, 4j)
	    GoodIdbJets.clear();
	    for (auto & i : GoodIdJets) { GoodIdbJets.push_back(i);}
	    for (auto & i : SVs) {  GoodIdbJets.push_back(i); }
	  } else if(tag_subcat.Contains("SR")) {  
	    isSignalRegion=true;
	  } else {
	    printf("UNDEFINED event category; please check!!\n");
	  }

	  if (ivar==0) {
	    if (passMet25 && passMt) {  
	      if (tag_subcat=="SR_3b") {
		mon.fillHisto("eventflow",tag_cat,6,weight);
	      }
	      if (tag_subcat=="SR_4b") {
		mon.fillHisto("eventflow",tag_cat,7,weight);
	      }
	    }
	  }
	  
	  //##############################################
	  //########  Main Event Selection        ########
	  //##############################################

	  // Here define all variables 
	  LorentzVector allHadronic;
	  //std::pair <int,LorentzVector> pairHadronic;
	  
	  // HT from all CSV + soft b's
	  float ht(0.); 
	  
	  // Hadronic vector sum:
	  int countb(0);
	  for (auto & thisb : GoodIdbJets) {
	    allHadronic+=thisb;
	    countb++; if (countb>3) break;
	  }
	  // Hadronic scalar sum (HT):
	  for (auto & thisb : GoodIdbJets) {
	    ht+=thisb.pt();
	  }
	  

	  //-----------------------------------------------------------
	  // Control plots
	  //-----------------------------------------------------------
	  
	  float dRave_(0.);
	  // DR(bb)_average
	  vector<float> dRs;
	  float dm(0.);
	  // Dphi(W,h) instead of DRmin(l,b)
	  double  dphi_Wh=fabs(deltaPhi(allHadronic.phi(),wsum.phi()));
	  
	  if (GoodIdbJets.size()==3) {
	    dRs.push_back(deltaR(GoodIdbJets[0],GoodIdbJets[1]));
	    dRs.push_back(deltaR(GoodIdbJets[0],GoodIdbJets[2]));
	    dRs.push_back(deltaR(GoodIdbJets[1],GoodIdbJets[2]));
	  }
	  else if (GoodIdbJets.size()>=4) 
	    {
	      dRs.push_back(deltaR(GoodIdbJets[0],GoodIdbJets[3]));
	      dRs.push_back(deltaR(GoodIdbJets[1],GoodIdbJets[3]));
	      dRs.push_back(deltaR(GoodIdbJets[2],GoodIdbJets[3]));
	      
	      float dm1 = fabs( (GoodIdbJets[0]+GoodIdbJets[1]).mass() - (GoodIdbJets[2]+GoodIdbJets[3]).mass() );
	      float dm2 = fabs( (GoodIdbJets[0]+GoodIdbJets[2]).mass() - (GoodIdbJets[1]+GoodIdbJets[3]).mass() );
	      
	      dm1 = min(dm1, dm2);
	      dm2 = fabs( (GoodIdbJets[0]+GoodIdbJets[3]).mass() - (GoodIdbJets[1]+GoodIdbJets[2]).mass() );
	      dm = min(dm1, dm2);
	    }
	  
	  //	  float dRave_(0.);
	  for (auto & it : dRs)
	    {
	      dRave_+=it;
	    }
	  dRave_/=dRs.size();
	  
	  // More MVA input variables
	  // dijet invariatn mass:
	  //	  float mjj=(GoodIdbJets[0]+GoodIdbJets[1]).mass();
	  //	  float ptjj=(GoodIdbJets[0]+GoodIdbJets[1]).pt();
	  //	  float dRll=deltaR(GoodIdbJets[0],SelLeptons[1]);
	  
	  //##############################################################################
	  //############ MVA Reader #####################################################
	  //##############################################################################
	  float mvaBDT(-10.0);

	  //	  if (GoodIdbJets.size() == 3)
	  if (tag_subcat.Contains("3b"))
	    {
	      mvaBDT = myTribTMVAReader.GenReMVAReader
		(
		 wsum.pt(), allHadronic.mass(), allHadronic.pt(), dRave_, dm, ht, dphi_Wh,
		 selLeptons[0].pt(),
		 imet.pt(), sqrt(tMass), mindphijmet,     
		 "Haa4bSBClassificationTribMVA"
		 );
	    }
	  else if (tag_subcat.Contains("4b")) 
	    //(GoodIdbJets.size() >= 4)
	    {
	      mvaBDT = myQuabTMVAReader.GenReMVAReader
		(
		 wsum.pt(), allHadronic.mass(), allHadronic.pt(), dRave_, dm, ht, dphi_Wh,
		 selLeptons[0].pt(),         
		 imet.pt(), sqrt(tMass), mindphijmet,     
		 "Haa4bSBClassificationQuabMVA"
		 );
	    }

	  //##############################################################################
	  //##############################################################################

	  if (ivar==0) {

	    // Reject QCD with Dphi(jet,MET) ?
	    float dphij1met=fabs(deltaPhi(GoodIdJets[0].phi(),imet.phi()));       
	    float dphij2met=fabs(deltaPhi(GoodIdJets[1].phi(),imet.phi()));
	    float min_dphijmet=min(dphij1met,dphij2met);
	    //mon.fillHisto("dphijmet12","raw_minj1j2",min_dphijmet,weight);
	    
	    // dphi(jet,MET)
	    mon.fillHisto("dphijmet",tags,mindphijmet,weight);
	    mon.fillHisto("dphijmet12",tags,min_dphijmet,weight); 
	    mon.fillHisto("dphijmet1",tags,dphij1met,weight); 
	    
	    // pu reweighting
	    mon.fillHisto("nvtx_raw",   tags, phys.nvtx,      xsecWeight*genWeight);
	    mon.fillHisto("nvtxwgt_raw",tags, phys.nvtx,      weight);
	    
	    // Lepton kinematics
	    mon.fillHisto("leadlep_pt_raw",tags,selLeptons[0].pt(),weight);
	    mon.fillHisto("leadlep_eta_raw",tags,selLeptons[0].eta(),weight);
	    
	    //lepton iso
	    mon.fillHisto("lep_reliso",tags,myrelIso,weight);    
	    //	      mon.fillHisto("lep_reliso",tags,mytrkrelIso,weight); 
	    
	    //zll mass
	    if(runZH){
	      mon.fillHisto("zmass_raw" ,tags, (selLeptons[0]+selLeptons[1]).mass(), weight);
	      mon.fillHisto("lep_pt_raw",tags,selLeptons[1].pt(),weight);
	      mon.fillHisto("lep_eta_raw",tags,selLeptons[1].eta(),weight);  
	    }
	    
	    // soft-bs + b-tagged jets
	    mon.fillHisto("nbtags_raw",tags,SVs.size(),weight);    
	    mon.fillHisto("nbjets_raw",tags, CSVLoosebJets.size(),weight);  
	    
	    // higgs mass
	    mon.fillHisto("higgsMass",tags,allHadronic.mass(),weight);
	    // higgs pT
	    mon.fillHisto("higgsPt",tags,allHadronic.pt(),weight);
	    // HT from all CSV + soft b's
	    mon.fillHisto("ht",tags,ht,weight);
	    // MET
	    mon.fillHisto("pfmet",tags,imet.pt(),weight);
	    // pTW
	    //LorentzVector wsum=imet+selLeptons[0];
	    mon.fillHisto("ptw",tags,wsum.pt(),weight);
	    // mtW 
	    mon.fillHisto("mtw",tags,sqrt(tMass),weight);
	    
	    // Dphi(W,h) 
	    mon.fillHisto("dphiWh",tags,dphi_Wh,weight);
	    // DRave, DMmin
	    mon.fillHisto("dRave",tags,dRave_,weight);
	    mon.fillHisto("dmmin",tags,dm, weight);
	    // BDT
	    mon.fillHisto("bdt", tags, mvaBDT, weight);
	      
	    
	    //##############################################################################
	    //############ MVA Handler ####################################################
	    //##############################################################################
	    
	    if (runMVA) {
	      
	      float mvaweight = 1.0;
	      genWeight > 0 ? mvaweight = weight/xsecWeight : mvaweight = -weight / xsecWeight; // Include all weights except for the xsecWeight
	      if ( isSignalRegion && GoodIdbJets.size() >= 3) 
		{
		  if(passMet25 && passMt) {
		    myMVAHandler_.getEntry
		      (
		       //	       GoodIdbJets.size() == 3, GoodIdbJets.size() >= 4, // 3b cat, 4b cat
		       tag_subcat == "SR_3b", tag_subcat == "SR_4b" , 
		       wsum.pt(), //W only, w pt
		       allHadronic.mass(), allHadronic.pt(), dRave_, dm, ht, //Higgs only, higgs mass, higgs pt, bbdr average, bb dm min, sum pt from all bs
		       dphi_Wh, //W and H, dr 
		       selLeptons[0].pt(),
		       imet.pt(), sqrt(tMass), mindphijmet,
		       mvaweight, //note, since weight is not the weight we want, we store all others except xSec weigh
		       ev.lheNJets //AUX variable for weight calculation
		       );
		    myMVAHandler_.fillTree();
		  }
		}
	    } // runMVA
	    
	  }// ivar==0
	  
	  //##############################################################################
	  //### HISTOS FOR STATISTICAL ANALYSIS (include systematic variations)
	  //##############################################################################

	  //	  if(passMet25 && passMt && passNJ2) {
	  //	  if (passZmass && passNJ2) {
	  
	    //scan the BDT cut and fill the shapes
	    for(unsigned int index=0;index<optim_Cuts1_bdt.size();index++){
	      if(mvaBDT>optim_Cuts1_bdt[index]){
		mon.fillHisto(TString("bdt_shapes")+varNames[ivar],tags,index, mvaBDT,weight);
		if (ivar==0) {
		  mon.fillHisto(TString("higgsMass_shapes")+varNames[ivar],tags,index, allHadronic.mass(),weight);  
		  mon.fillHisto(TString("higgsPt_shapes")+varNames[ivar],tags,index, allHadronic.pt(),weight);        
		  mon.fillHisto(TString("ht_shapes")+varNames[ivar],tags,index, ht,weight);       
		  mon.fillHisto(TString("pfmet_shapes")+varNames[ivar],tags,index, imet.pt(),weight);       
		  mon.fillHisto(TString("ptw_shapes")+varNames[ivar],tags,index, wsum.pt(),weight);       
		  mon.fillHisto(TString("mtw_shapes")+varNames[ivar],tags,index, sqrt(tMass),weight);
		  mon.fillHisto(TString("dphiWh_shapes")+varNames[ivar],tags,index, dphi_Wh,weight);         
		  mon.fillHisto(TString("dRave_shapes")+varNames[ivar],tags,index, dRave_,weight);
		  mon.fillHisto(TString("dmmin_shapes")+varNames[ivar],tags,index, dm,weight);             
		  mon.fillHisto(TString("dphijmet_shapes")+varNames[ivar],tags,index,mindphijmet,weight);
		  mon.fillHisto(TString("lep_pt_raw_shapes")+varNames[ivar],tags,index,selLeptons[0].pt(),weight);
		}
	      } // run slimmed analysis
	    }
	    
	    //	  } // END SELECTION
	  
	} // Systematic variation END
	
    } // loop on all events END
    
    PU_Central_File->Close(); 
    PU_Up_File->Close(); 
    PU_Down_File->Close(); 

    E_TRG_SF_file->Close(); E_RECO_SF_file->Close(); 
    E_TIGHTID_SF_file->Close();
    MU_TRG_SF_file->Close();
   
    if(nMethod == 2){
      f_CSVwgt_HF->Close();
      f_CSVwgt_LF->Close();	    
    }
//    btagfile->Close();

    printf("\n");
    file->Close();

    //write MVA files
    if (runMVA) { myMVAHandler_.writeTree(); }

    //##############################################
    //########     SAVING HISTO TO FILE     ########
    //##############################################
    //save control plots to file
    outUrl += "/";
    outUrl += outFileUrl + ".root";
//    outUrl = outFileUrl + ".root";
    printf("Results saved in %s\n", outUrl.Data());

    //save all to the file
    int nTrial = 0;
    TFile *ofile=TFile::Open(outUrl, "recreate");
    while( !ofile->IsOpen() || ofile->IsZombie() ){
        if(nTrial > 3){
          printf("Output file open failed!");
          if ( outTxtFile_final ) fclose(outTxtFile_final);
          return -1;
        }
        nTrial++;
        usleep(1000000*nTrial);
        ofile=TFile::Open(outUrl, "update");
    }
    mon.Write();
    ofile->Close();

    if ( outTxtFile_final ) fclose(outTxtFile_final);
}

// fill the histograms (done once)
// https://twiki.cern.ch/twiki/bin/view/CMS/BTagShapeCalibration
void fillCSVhistos(TFile* fileHF, TFile* fileLF){
  for( int iSys=0; iSys<9; iSys++ ){
    for( int iPt=0; iPt<5; iPt++ ) h_csv_wgt_hf[iSys][iPt] = NULL;
    for( int iPt=0; iPt<3; iPt++ ){
      for( int iEta=0; iEta<3; iEta++ )h_csv_wgt_lf[iSys][iPt][iEta] = NULL;
    }
  }
  for( int iSys=0; iSys<5; iSys++ ){
    for( int iPt=0; iPt<5; iPt++ ) c_csv_wgt_hf[iSys][iPt] = NULL;
  }

  // CSV reweighting /// only care about the nominal ones
  for( int iSys=0; iSys<9; iSys++ ){
    TString syst_csv_suffix_hf = "final";
    TString syst_csv_suffix_c = "final";
    TString syst_csv_suffix_lf = "final";

    switch(iSys){
    case 0:
      // this is the nominal case
      break;
    case 1:
      // JESUp
      syst_csv_suffix_hf = "final_JESUp"; syst_csv_suffix_lf = "final_JESUp";
      syst_csv_suffix_c  = "final_cErr1Up";
      break;
    case 2:
      // JESDown
      syst_csv_suffix_hf = "final_JESDown"; syst_csv_suffix_lf = "final_JESDown";
      syst_csv_suffix_c  = "final_cErr1Down";
      break;
    case 3:
      // purity up
      syst_csv_suffix_hf = "final_LFUp"; syst_csv_suffix_lf = "final_HFUp";
      syst_csv_suffix_c  = "final_cErr2Up";
      break;
    case 4:
      // purity down
      syst_csv_suffix_hf = "final_LFDown"; syst_csv_suffix_lf = "final_HFDown";
      syst_csv_suffix_c  = "final_cErr2Down";
      break;
    case 5:
      // stats1 up
      syst_csv_suffix_hf = "final_Stats1Up"; syst_csv_suffix_lf = "final_Stats1Up";
      break;
    case 6:
      // stats1 down
      syst_csv_suffix_hf = "final_Stats1Down"; syst_csv_suffix_lf = "final_Stats1Down";
      break;
    case 7:
      // stats2 up
      syst_csv_suffix_hf = "final_Stats2Up"; syst_csv_suffix_lf = "final_Stats2Up";
      break;
    case 8:
      // stats2 down
      syst_csv_suffix_hf = "final_Stats2Down"; syst_csv_suffix_lf = "final_Stats2Down";
      break;
    }

    for( int iPt=0; iPt<5; iPt++ ) {h_csv_wgt_hf[iSys][iPt] = (TH1D*)fileHF->Get( Form("csv_ratio_Pt%i_Eta0_%s",iPt,syst_csv_suffix_hf.Data()) );}

    if( iSys<5 ){
      for( int iPt=0; iPt<5; iPt++ ) {c_csv_wgt_hf[iSys][iPt] = (TH1D*)fileHF->Get( Form("c_csv_ratio_Pt%i_Eta0_%s",iPt,syst_csv_suffix_c.Data()) );}
    }

    for( int iPt=0; iPt<4; iPt++ ){
      for( int iEta=0; iEta<3; iEta++ ) {h_csv_wgt_lf[iSys][iPt][iEta] = (TH1D*)fileLF->Get( Form("csv_ratio_Pt%i_Eta%i_%s",iPt,iEta,syst_csv_suffix_lf.Data()) );}
    }
  }

  return;
}

double get_csv_wgt(double jetPt, double JetEta, double csv, int flavor, int iSys, double &csvWgtHF, double &csvWgtLF, double &csvWgtCF){

  int iSysHF = 0;
  switch(iSys){
    case 7:  iSysHF=1; break; //JESUp
    case 8:  iSysHF=2; break; //JESDown
    case 9:  iSysHF=3; break; //LFUp
    case 10: iSysHF=4; break; //LFDown
    case 13: iSysHF=5; break; //Stats1Up
    case 14: iSysHF=6; break; //Stats1Down
    case 15: iSysHF=7; break; //Stats2Up
    case 16: iSysHF=8; break; //Stats2Down
    default : iSysHF = 0; break; //NoSys
  }

  int iSysC = 0;
  switch(iSys){
    case 21: iSysC=1; break;
    case 22: iSysC=2; break;
    case 23: iSysC=3; break;
    case 24: iSysC=4; break;
    default : iSysC = 0; break;
   }

  int iSysLF = 0;
  switch(iSys){
    case 7:  iSysLF=1; break; //JESUp
    case 8:  iSysLF=2; break; //JESDown
    case 11: iSysLF=3; break; //HFUp
    case 12: iSysLF=4; break; //HFDown
    case 17: iSysLF=5; break; //Stats1Up
    case 18: iSysLF=6; break; //Stats1Down
    case 19: iSysLF=7; break; //Stats2Up
    case 20: iSysLF=8; break; //Stats2Down
    default : iSysLF = 0; break; //NoSys
  }

  csvWgtHF = 1.;
  csvWgtLF = 1.;
  csvWgtCF = 1.;

  double jetAbsEta = abs(csvWgtCF);
  int iPt = -1; int iEta = -1;

  if (jetPt >=19.99 && jetPt<30) iPt = 0;
  else if (jetPt >=30 && jetPt<40) iPt = 1;
  else if (jetPt >=40 && jetPt<60) iPt = 2;
  else if (jetPt >=60 && jetPt<100) iPt = 3;
  else if (jetPt >=100) iPt = 4;

  if (jetAbsEta >=0 &&  jetAbsEta<0.8 ) iEta = 0;
  else if ( jetAbsEta>=0.8 && jetAbsEta<1.6 )  iEta = 1;
  else if ( jetAbsEta>=1.6 && jetAbsEta<2.41 ) iEta = 2;

  if (iPt < 0 || iEta < 0) std::cout << "Error, couldn't find Pt, Eta bins for this b-flavor jet, jetPt = " << jetPt << ", jetAbsEta = " << jetAbsEta << std::endl;

  if(abs(flavor)==5){
    int useCSVBin = (csv>=0.) ? h_csv_wgt_hf[iSysHF][iPt]->FindBin(csv) : 1;
    double iCSVWgtHF = h_csv_wgt_hf[iSysHF][iPt]->GetBinContent(useCSVBin);
    if( iCSVWgtHF!=0 ) csvWgtHF *= iCSVWgtHF;
  }
  else if( abs(flavor) == 4 ){
    int useCSVBin = (csv>=0.) ? c_csv_wgt_hf[iSysC][iPt]->FindBin(csv) : 1;
    double iCSVWgtC = c_csv_wgt_hf[iSysC][iPt]->GetBinContent(useCSVBin);
    if( iCSVWgtC!=0 ) csvWgtCF *= iCSVWgtC;
  }
  else{
    if (iPt >=3) iPt=3;       /// [30-40], [40-60] and [60-10000] only 3 Pt bins for lf
    int useCSVBin = (csv>=0.) ? h_csv_wgt_lf[iSysLF][iPt][iEta]->FindBin(csv) : 1;
    double iCSVWgtLF = h_csv_wgt_lf[iSysLF][iPt][iEta]->GetBinContent(useCSVBin);
    if( iCSVWgtLF!=0 ) csvWgtLF *= iCSVWgtLF;
  }
  double csvWgtTotal = csvWgtHF * csvWgtCF * csvWgtLF;
  return csvWgtTotal;
}
