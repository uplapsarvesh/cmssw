// -----------------------------
// 
// Offline DQM for razor triggers. The razor inclusive analysis measures trigger efficiency
// in SingleElectron events (orthogonal to analysis), as a 2D function of the razor variables
// M_R and R^2. Also monitor dPhi_R, used offline for  QCD and/or detector-related MET tail 
// rejection.
// Based on DQMOffline/Trigger/plugins/METMonitor.*
//
// -----------------------------

#include "DQMOffline/Trigger/plugins/RazorMonitor.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "CommonTools/TriggerUtils/interface/GenericTriggerEventFlag.h"

// -----------------------------
//  constructors and destructor
// -----------------------------

RazorMonitor::RazorMonitor( const edm::ParameterSet& iConfig ) : 
  folderName_             ( iConfig.getParameter<std::string>("FolderName") )
  , metToken_             ( consumes<reco::PFMETCollection>      (iConfig.getParameter<edm::InputTag>("met")       ) )   
  , jetToken_             ( mayConsume<reco::PFJetCollection>      (iConfig.getParameter<edm::InputTag>("jets")      ) )   
  , rsq_binning_          ( iConfig.getParameter<edm::ParameterSet>("histoPSet").getParameter<std::vector<double> >   ("rsqBins")    )
  , mr_binning_           ( iConfig.getParameter<edm::ParameterSet>("histoPSet").getParameter<std::vector<double> >   ("mrBins")     )
  , dphiR_binning_        ( iConfig.getParameter<edm::ParameterSet>("histoPSet").getParameter<std::vector<double> >   ("dphiRBins")  )
  , num_genTriggerEventFlag_(new GenericTriggerEventFlag(iConfig.getParameter<edm::ParameterSet>("numGenericTriggerEventPSet"),consumesCollector(), *this))
  , den_genTriggerEventFlag_(new GenericTriggerEventFlag(iConfig.getParameter<edm::ParameterSet>("denGenericTriggerEventPSet"),consumesCollector(), *this))
  , metSelection_ ( iConfig.getParameter<std::string>("metSelection") )
  , jetSelection_ ( iConfig.getParameter<std::string>("jetSelection") )
  , njets_      ( iConfig.getParameter<unsigned int>("njets" )      )
  , rsqCut_     ( iConfig.getParameter<double>("rsqCut" )  )
  , mrCut_      ( iConfig.getParameter<double>("mrCut" )   )
{

  theHemispheres_ = consumes<std::vector<math::XYZTLorentzVector> >(iConfig.getParameter<edm::InputTag>("hemispheres"));

  MR_ME_.numerator = nullptr;
  MR_ME_.denominator = nullptr;
  Rsq_ME_.numerator = nullptr;
  Rsq_ME_.denominator = nullptr;
  dPhiR_ME_.numerator = nullptr;
  dPhiR_ME_.denominator = nullptr;

  MRVsRsq_ME_.numerator = nullptr;
  MRVsRsq_ME_.denominator = nullptr;

}

RazorMonitor::~RazorMonitor()
{
}

void RazorMonitor::setMETitle(RazorME& me, std::string titleX, std::string titleY)
{
  me.numerator->setAxisTitle(titleX,1);
  me.numerator->setAxisTitle(titleY,2);
  me.denominator->setAxisTitle(titleX,1);
  me.denominator->setAxisTitle(titleY,2);

}

void RazorMonitor::bookME(DQMStore::IBooker &ibooker, RazorME& me, const std::string& histname, const std::string& histtitle, int nbins, double min, double max)
{
  me.numerator   = ibooker.book1D(histname+"_numerator",   histtitle+" (numerator)",   nbins, min, max);
  me.denominator = ibooker.book1D(histname+"_denominator", histtitle+" (denominator)", nbins, min, max);
}
void RazorMonitor::bookME(DQMStore::IBooker &ibooker, RazorME& me, const std::string& histname, const std::string& histtitle, const std::vector<double>& binning)
{
  int nbins = binning.size()-1;
  std::vector<float> fbinning(binning.begin(),binning.end());
  float* arr = &fbinning[0];
  me.numerator   = ibooker.book1D(histname+"_numerator",   histtitle+" (numerator)",   nbins, arr);
  me.denominator = ibooker.book1D(histname+"_denominator", histtitle+" (denominator)", nbins, arr);
}
void RazorMonitor::bookME(DQMStore::IBooker &ibooker, RazorME& me, const std::string& histname, const std::string& histtitle, int nbinsX, double xmin, double xmax, double ymin, double ymax)
{
  me.numerator   = ibooker.bookProfile(histname+"_numerator",   histtitle+" (numerator)",   nbinsX, xmin, xmax, ymin, ymax);
  me.denominator = ibooker.bookProfile(histname+"_denominator", histtitle+" (denominator)", nbinsX, xmin, xmax, ymin, ymax);
}
void RazorMonitor::bookME(DQMStore::IBooker &ibooker, RazorME& me, const std::string& histname, const std::string& histtitle, int nbinsX, double xmin, double xmax, int nbinsY, double ymin, double ymax)
{
  me.numerator   = ibooker.book2D(histname+"_numerator",   histtitle+" (numerator)",   nbinsX, xmin, xmax, nbinsY, ymin, ymax);
  me.denominator = ibooker.book2D(histname+"_denominator", histtitle+" (denominator)", nbinsX, xmin, xmax, nbinsY, ymin, ymax);
}
void RazorMonitor::bookME(DQMStore::IBooker &ibooker, RazorME& me, const std::string& histname, const std::string& histtitle, const std::vector<double>& binningX, const std::vector<double>& binningY)
{
  int nbinsX = binningX.size()-1;
  std::vector<float> fbinningX(binningX.begin(),binningX.end());
  float* arrX = &fbinningX[0];
  int nbinsY = binningY.size()-1;
  std::vector<float> fbinningY(binningY.begin(),binningY.end());
  float* arrY = &fbinningY[0];

  me.numerator   = ibooker.book2D(histname+"_numerator",   histtitle+" (numerator)",   nbinsX, arrX, nbinsY, arrY);
  me.denominator = ibooker.book2D(histname+"_denominator", histtitle+" (denominator)", nbinsX, arrX, nbinsY, arrY);
}

void RazorMonitor::bookHistograms(DQMStore::IBooker     & ibooker,
				 edm::Run const        & iRun,
				 edm::EventSetup const & iSetup) 
{  
  
  std::string histname, histtitle;

  std::string currentFolder = folderName_ ;
  ibooker.setCurrentFolder(currentFolder.c_str());

  // 1D hist, MR
  histname = "MR"; histtitle = "PF MR";
  bookME(ibooker,MR_ME_,histname,histtitle,mr_binning_);
  setMETitle(MR_ME_,"PF M_{R} [GeV]","events / [GeV]");

  // 1D hist, Rsq
  histname = "Rsq"; histtitle = "PF Rsq";
  bookME(ibooker,Rsq_ME_,histname,histtitle,rsq_binning_);
  setMETitle(Rsq_ME_,"PF R^{2}","events");

  // 1D hist, dPhiR
  histname = "dPhiR"; histtitle = "dPhiR";
  bookME(ibooker,dPhiR_ME_,histname,histtitle,dphiR_binning_);
  setMETitle(dPhiR_ME_,"dPhi_{R}","events");

  // 2D hist, MR & Rsq
  histname = "MRVsRsq"; histtitle = "PF MR vs PF Rsq";
  bookME(ibooker,MRVsRsq_ME_,histname,histtitle,mr_binning_, rsq_binning_);
  setMETitle(MRVsRsq_ME_,"M_{R} [GeV]","R^{2}");

  // Initialize the GenericTriggerEventFlag
  if ( num_genTriggerEventFlag_ && num_genTriggerEventFlag_->on() ) num_genTriggerEventFlag_->initRun( iRun, iSetup );
  if ( den_genTriggerEventFlag_ && den_genTriggerEventFlag_->on() ) den_genTriggerEventFlag_->initRun( iRun, iSetup );

}

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"
void RazorMonitor::analyze(edm::Event const& iEvent, edm::EventSetup const& iSetup)  {

  // Filter out events if Trigger Filtering is requested
  if (den_genTriggerEventFlag_->on() && ! den_genTriggerEventFlag_->accept( iEvent, iSetup) ) return;

  //met collection
  edm::Handle<reco::PFMETCollection> metHandle;
  iEvent.getByToken( metToken_, metHandle );
  reco::PFMET pfmet = metHandle->front();
  if ( ! metSelection_( pfmet ) ) return;

  //jet collection, track # of jets for two working points
  edm::Handle<reco::PFJetCollection> jetHandle;
  iEvent.getByToken( jetToken_, jetHandle );
  std::vector<reco::PFJet> jets;
  if ( jetHandle->size() < njets_ ) return;
  for ( auto const & j : *jetHandle ) {
    if ( jetSelection_( j ) ) jets.push_back(j);
  }
  if ( jets.size() < njets_ ) return;

  //razor hemisphere clustering from previous step
  edm::Handle< vector<math::XYZTLorentzVector> > hemispheres;
  iEvent.getByToken (theHemispheres_,hemispheres);

  if (not hemispheres.isValid()){
    return;
  }

  if(hemispheres->size() ==0){  // the Hemisphere Maker will produce an empty collection of hemispheres if # of jets is too high
    edm::LogError("DQM_HLT_Razor") << "Cannot calculate M_R and R^2 because there are too many jets! (trigger passed automatically without forming the hemispheres)" << endl;
    return;
  }

  // should always have 2 hemispheres -- no muons included (c. 2017), if not return invalid hemisphere collection
  // retaining check for hemisphere size 5 or 10 which correspond to the one or two muon case for possible future use
  if(hemispheres->size() != 0 && hemispheres->size() != 2 && hemispheres->size() != 5 && hemispheres->size() != 10){
    edm::LogError("DQM_HLT_Razor") << "Invalid hemisphere collection!  hemispheres->size() = " << hemispheres->size() << endl;
    return;
  }

  //calculate razor variables, with hemispheres pT-ordered
  double MR = 0, R = 0;
  if (hemispheres->at(1).Pt() > hemispheres->at(0).Pt()) {
    MR = CalcMR(hemispheres->at(1),hemispheres->at(0));
    R = CalcR(MR,hemispheres->at(1),hemispheres->at(0),metHandle);
  }
  else {
    MR = CalcMR(hemispheres->at(0),hemispheres->at(1));
    R = CalcR(MR,hemispheres->at(0),hemispheres->at(1),metHandle);
  }
  
  double Rsq = R*R;
  double dPhiR = abs(deltaPhi(hemispheres->at(0).Phi(), hemispheres->at(1).Phi()));

  //apply offline selection cuts
  if (Rsq<rsqCut_ && MR<mrCut_) return;

  // applying selection for denominator
  if (den_genTriggerEventFlag_->on() && ! den_genTriggerEventFlag_->accept( iEvent, iSetup) ) return;

  // filling histograms (denominator)
  if (Rsq>=rsqCut_) {
    MR_ME_.denominator -> Fill(MR);
  }

  if (MR>=mrCut_) {
    Rsq_ME_.denominator -> Fill(Rsq);
  }

  dPhiR_ME_.denominator -> Fill(dPhiR);

  MRVsRsq_ME_.denominator -> Fill(MR, Rsq);

  // applying selection for numerator
  if (num_genTriggerEventFlag_->on() && ! num_genTriggerEventFlag_->accept( iEvent, iSetup) ) return;

  // filling histograms (numerator)
  if (Rsq>=rsqCut_) {
    MR_ME_.numerator -> Fill(MR);
  }

  if (MR>=mrCut_) {
    Rsq_ME_.numerator -> Fill(Rsq);
  }

  dPhiR_ME_.numerator -> Fill(dPhiR);

  MRVsRsq_ME_.numerator -> Fill(MR, Rsq);

}

void RazorMonitor::fillDescriptions(edm::ConfigurationDescriptions & descriptions)
{
  edm::ParameterSetDescription desc;
  desc.add<std::string>  ( "FolderName", "HLT/SUSY/Razor" );

  desc.add<edm::InputTag>( "met",      edm::InputTag("pfMet") );
  desc.add<edm::InputTag>( "jets",     edm::InputTag("ak4PFJetsCHS") );
  desc.add<edm::InputTag>("hemispheres",edm::InputTag("hemispheresDQM"))->setComment("hemisphere jets used to compute razor variables");
  desc.add<std::string>("metSelection", "pt > 0");

  // from 2016 offline selection
  desc.add<std::string>("jetSelection", "pt > 80");
  desc.add<unsigned int>("njets",      2);
  desc.add<double>("mrCut",    300);
  desc.add<double>("rsqCut",   0.15);

  edm::ParameterSetDescription genericTriggerEventPSet;
  genericTriggerEventPSet.add<bool>("andOr");
  genericTriggerEventPSet.add<edm::InputTag>("dcsInputTag", edm::InputTag("scalersRawToDigi") );
  genericTriggerEventPSet.add<std::vector<int> >("dcsPartitions",{});
  genericTriggerEventPSet.add<bool>("andOrDcs", false);
  genericTriggerEventPSet.add<bool>("errorReplyDcs", true);
  genericTriggerEventPSet.add<std::string>("dbLabel","");
  genericTriggerEventPSet.add<bool>("andOrHlt", true);
  genericTriggerEventPSet.add<edm::InputTag>("hltInputTag", edm::InputTag("TriggerResults::HLT") );
  genericTriggerEventPSet.add<std::vector<std::string> >("hltPaths",{});
  genericTriggerEventPSet.add<std::string>("hltDBKey","");
  genericTriggerEventPSet.add<bool>("errorReplyHlt",false);
  genericTriggerEventPSet.add<unsigned int>("verbosityLevel",1);

  desc.add<edm::ParameterSetDescription>("numGenericTriggerEventPSet", genericTriggerEventPSet);
  desc.add<edm::ParameterSetDescription>("denGenericTriggerEventPSet", genericTriggerEventPSet);

  //binning from 2016 offline selection
  edm::ParameterSetDescription histoPSet;
  std::vector<double> mrbins = {0., 100., 200., 300., 400., 500., 575., 650., 750., 900., 1200., 1600., 2500., 4000.};
  histoPSet.add<std::vector<double> >("mrBins", mrbins);

  std::vector<double> rsqbins = {0., 0.05, 0.1, 0.15, 0.2, 0.25, 0.30, 0.41, 0.52, 0.64, 0.8, 1.5};
  histoPSet.add<std::vector<double> >("rsqBins", rsqbins);

  std::vector<double> dphirbins = {0., 0.5, 1.0, 1.5, 2.0, 2.5, 2.8, 3.0, 3.2};
  histoPSet.add<std::vector<double> >("dphiRBins", dphirbins);

  desc.add<edm::ParameterSetDescription>("histoPSet",histoPSet);

  descriptions.add("razorMonitoring", desc);
}

//CalcMR and CalcR borrowed from HLTRFilter.cc
double RazorMonitor::CalcMR(const math::XYZTLorentzVector& ja, const math::XYZTLorentzVector& jb){
  if(ja.Pt()<=0.1) return -1;

  double A = ja.P();
  double B = jb.P();
  double az = ja.Pz();
  double bz = jb.Pz();
  TVector3 jaT, jbT;
  jaT.SetXYZ(ja.Px(),ja.Py(),0.0);
  jbT.SetXYZ(jb.Px(),jb.Py(),0.0);
  double ATBT = (jaT+jbT).Mag2();

  double MR = sqrt((A+B)*(A+B)-(az+bz)*(az+bz)-
                   (jbT.Dot(jbT)-jaT.Dot(jaT))*(jbT.Dot(jbT)-jaT.Dot(jaT))/(jaT+jbT).Mag2());
  double mybeta = (jbT.Dot(jbT)-jaT.Dot(jaT))/
    sqrt(ATBT*((A+B)*(A+B)-(az+bz)*(az+bz)));

  double mygamma = 1./sqrt(1.-mybeta*mybeta);

  //use gamma times MRstar
  return MR*mygamma;
}

double RazorMonitor::CalcR(double MR, const math::XYZTLorentzVector& ja, const math::XYZTLorentzVector& jb, const edm::Handle<std::vector<reco::PFMET> >& inputMet){

  //now we can calculate MTR
  const math::XYZVector met = (inputMet->front()).momentum();

  double MTR = sqrt(0.5*(met.R()*(ja.Pt()+jb.Pt()) - met.Dot(ja.Vect()+jb.Vect())));

  //filter events
  return float(MTR)/float(MR); //R
}

// Define this as a plug-in
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(RazorMonitor);
