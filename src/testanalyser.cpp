#include <vector>
#include "interface/testanalyser.h"
#include "../interface/helpers.h"
#include "../interface/ttbar_solver.h"
#include "../interface/pzCalculator.h"
#include "TMath.h"
using namespace std;

double M_mu =  0.10566;
double M_e = 0.511e-3;

void testanalyser::analyze(size_t childid /* this info can be used for printouts */){

	d_ana::dBranchHandler<Electron> elecs(tree(),"Electron");
	d_ana::dBranchHandler<HepMCEvent>  event(tree(),"Event");
	d_ana::dBranchHandler<Jet>         genjet(tree(),"GenJet");
	d_ana::dBranchHandler<Jet>         jet(tree(),"Jet");
	d_ana::dBranchHandler<Muon>        muontight(tree(),"Muon");
	d_ana::dBranchHandler<MissingET>   met(tree(),"MissingET");

	TH1* neutrinoMomentumHisto = addPlot(new TH1D("neutrinoMomentumHisto", "nu histo",100,0,200), "pT","N");
	TH1* histoSL=addPlot(new TH1D("tquark_SL","t-quark m_{inv} mass in semileptonic decay",50,100,400),"M [GeV]","N");
	TH1* topMHistoBoost=addPlot(new TH1D("topMHistoBoost","t-quark m_{inv} mass",100,0,400),"M [GeV]","N");
	TH1* tbarMHistoBoost=addPlot(new TH1D("tbarMHistoBoost","antit-quark m_{inv} mass",100,0,400),"M [GeV]","N");
	TH1* massDiffHisto = addPlot(new TH1D("massDiffHisto", "m_t - m_{bar{t}}", 100, -50, 50), "#Delta m", "N_{events}");

	size_t nevents=tree()->entries();

	if(isTestMode())
		nevents/=100;

	for(size_t eventno=0; eventno<nevents; eventno++){
		/*
		 * The following two lines report the status and set the event link
		 * Do not remove!
		 */
		reportStatus(eventno,nevents);
		tree()->setEntry(eventno);

		//At least 2 b-jets and 2 light jets
		size_t bjets=0;
		size_t ljets=0;
		for(size_t i=0;i<jet.size();i++){
			if(fabs(jet.at(i)->Eta)<2.5){
				if(jet.at(i)->BTag){
					bjets++;
				}else{ljets++;}
			}
		}
		if(bjets<2){continue;}
		if(ljets<2){continue;}

		//determine the number of leptons with pt>30
		size_t nrOfHighPtLeps=0;
		size_t nrOfHighPtElecs=0;
		size_t nrOfHighPtMus=0;
		size_t lepIndex=0; // hold the value corresponding to the index of the high PT lepton
		double M_lepton = M_mu;
		for(size_t i=0; i<elecs.size(); i++){
			if (((elecs.at(i)->PT) > 30) && (TMath::Abs(elecs.at(i)->Eta) < 2.1)){
				nrOfHighPtLeps++;
				nrOfHighPtElecs++;
			}
		}	
		for(size_t i=0; i<muontight.size(); i++){
			if (((muontight.at(i)->PT) > 30) && (TMath::Abs(muontight.at(i)->Eta) < 2.1)){
				nrOfHighPtLeps++;
				nrOfHighPtMus++;
			}
		}	

		if (nrOfHighPtLeps != 1) continue;
		if (nrOfHighPtElecs==1){
			for(size_t i=0; i<elecs.size(); i++){
				if (elecs.at(i)->PT > 30){
					lepIndex = i;
				}
			}
		}else{
			for(size_t i=0; i<muontight.size(); i++){
				if (muontight.at(i)->PT > 30){
					lepIndex = i;
				}
			}
		}

		TLorentzVector lepVec(0.,0.,0.,0.);
		TLorentzVector elecVec(0.,0.,0.,0.);
		TLorentzVector muVec(0.,0.,0.,0.);
		bool antiLepton = false;
		double leptonPT, leptonEta, leptonPhi;

		if (nrOfHighPtElecs==1){
			M_lepton = M_e;
			leptonPT = elecs.at(lepIndex)->PT;
			leptonEta = elecs.at(lepIndex)->Eta;
			leptonPhi = elecs.at(lepIndex)->Phi;
			lepVec.SetPtEtaPhiM(leptonPT, leptonEta, leptonPhi , M_lepton);
			//elecVec.SetPtEtaPhiM(leptonPT, leptonEta, leptonPhi , M_lepton);
		//	pymu = elecVec.Py();
		//	pzmu = elecVec.Pz();
		//	lepVec = elecVec;
			if (elecs.at(lepIndex)->Charge == 1){
				antiLepton = true;
			}
		} else{
			leptonPT = muontight.at(lepIndex)->PT;
			leptonEta = muontight.at(lepIndex)->Eta;
			leptonPhi = muontight.at(lepIndex)->Phi;
			lepVec.SetPtEtaPhiM(leptonPT, leptonEta, leptonPhi , M_lepton);
			/*emu = muVec.E();
			pxmu = muVec.Px();
			pymu = muVec.Py();
			pzmu = muVec.Pz();
			lepVec = muVec;*/
			if (muontight.at(lepIndex)->Charge == 1){
				antiLepton = true;
			}
		}

		//finding neutrino mass from missing transverse energy
		TLorentzVector missingET(0.,0.,0.,0.);
		missingET=(met.at(0)->P4());
		pzCalculator calculator;
		calculator.setLepton(lepVec);
		calculator.setMET(missingET);
		calculator.setLepMass(M_lepton);
		double pznu = calculator.getPz();
		TLorentzVector neutrinoP4(missingET.Px(),
					missingET.Py(),
					pznu,
					TMath::Sqrt(TMath::Power(missingET.Pt(),2)+TMath::Power(pznu,2)));
		neutrinoMomentumHisto->Fill(neutrinoP4.Pt());

		TLorentzVector t(0., 0., 0., 0.); //top quark
		TLorentzVector tbar(0., 0., 0., 0.); // top antiquark
		TLorentzVector bJetVec(0.,0.,0.,0.);
		TLorentzVector empty(0.,0.,0.,0.);// an Lorentz vector that stays empty

		for (size_t i = 0; i<jet.size(); i++){
			if (acceptBJet(jet.at(i))){
				double DelR=0;
				if (nrOfHighPtElecs==1){
					DelR = deltaR(jet.at(i), elecs.at(lepIndex));
				}
				if (nrOfHighPtMus==1){
					DelR = deltaR(jet.at(i), muontight.at(lepIndex));
				}
				if (DelR<1.2){ // getting top mass from leptonic decay
					bJetVec = makeTLorentzVector(jet.at(i));
					if(antiLepton){t = bJetVec+lepVec+neutrinoP4;}
					else{tbar = bJetVec+lepVec+neutrinoP4;}
					double JetR=0;
					JetR = TMath::Sqrt(TMath::Power(jet.at(i)->DeltaEta,2)+TMath::Power(jet.at(i)->DeltaPhi,2));
					if(DelR<JetR && antiLepton){
						t = t - lepVec;
						}
					if(DelR<JetR && !antiLepton){
						tbar = tbar - lepVec;
					}


				}else{ // getting top mass from hadronic decay (copied from Aleksei) 
					if (t==empty && tbar==empty) continue;
					for(size_t j=0;j<jet.size();j++){
						if (acceptLJet(jet.at(j))){
							DelR = deltaR(jet.at(i),jet.at(j));
							if (DelR<1.2){
								for(size_t k=j;k<jet.size();k++){
									if(acceptLJet(jet.at(k))){
										size_t leading=0;
										if(jet.at(i)->PT > jet.at(j)->PT){
											leading=i;
										}else{leading=j;}

										if(jet.at(leading)->PT < jet.at(k)->PT){
											leading=k;
										}
										if(leading!=k){
											DelR=deltaR(jet.at(leading),jet.at(k));
										}else{
											DelR=deltaR(jet.at(i),jet.at(k));
											double DelR_2=0;
											DelR = deltaR(jet.at(j),jet.at(k));
											if(DelR<DelR_2){
												DelR=DelR_2;
											}
										}
										if(DelR<1.2){
											TLorentzVector jet1(0.,0.,0.,0.);
											jet1=makeTLorentzVector(jet.at(i));
											TLorentzVector jet2(0.,0.,0.,0.);
											jet2=makeTLorentzVector(jet.at(j));
											TLorentzVector jet3(0.,0.,0.,0.);
											jet3=makeTLorentzVector(jet.at(k));

											TLorentzVector topQ(0.,0.,0.,0.);
											if(antiLepton){
												tbar=jet1+jet2+jet3;
											}else{t=jet1+jet2+jet3;}
										}
									}
								}
							}
						}
					}
				
				}//else statement (corresponding to DelR>=1.2)
			}//if (acceptBJet) 
		}// jet loop

		//find masses and mass differences of t and tbar
		if (t.M()>0 && tbar.M()>0){
		
			massDiffHisto->Fill(t.M() - tbar.M());
			topMHistoBoost->Fill(t.M());
			tbarMHistoBoost->Fill(tbar.M());
		}
		
		
		// minimum chi^2
		ttbar_solver solver;	
		double topmass1 = 0;
		double topmass2 = 0;
		solver.setLepton(lepVec);
		solver.setNeutrino(neutrinoP4);
		double bestchi = 1000;
		for(size_t i=0; i<jet.size(); i++){
			//bjet from blv
			if(acceptBJet(jet.at(i))){
				TLorentzVector bjet1(0.,0.,0.,0.);
				TLorentzVector bjet2(0.,0.,0.,0.);
				TLorentzVector ljet1(0.,0.,0.,0.);
				TLorentzVector ljet2(0.,0.,0.,0.);
				bjet1=makeTLorentzVector(jet.at(i));
				solver.setBJetA(bjet1);
				for(size_t j=0; j<jet.size(); j++){
					//bjet from bqq'
					if(acceptBJet(jet.at(j)) && j!=i){
						bjet2=makeTLorentzVector(jet.at(j));
						solver.setBJetB(bjet2);
						for(size_t k=0;k<jet.size();k++){
							//first light jet
							if(acceptLJet(jet.at(k))){
								ljet1=makeTLorentzVector(jet.at(k));
								solver.setLightJetA(ljet1);
								for(size_t l=k;l<jet.size();l++){
									//second lightjet
									if(acceptLJet(jet.at(l)) && l!=k){
										ljet2=makeTLorentzVector(jet.at(l));
										solver.setLightJetB(ljet2);
										//comparing chi2 with previous combination
										double chi2=solver.getChi2();
										if(chi2<bestchi){
											bestchi=chi2;	
											topmass1=(bjet1+neutrinoP4+lepVec).M();
											topmass2=(bjet2+ljet1+ljet2).M();
										}
									}
								}
							}
						}
					}
				}
			}
		}//chi2 outer loop
		//histogram of semileptonic decay
		histoSL->Fill(topmass1); //from blv
		histoSL->Fill(topmass2); //from bqq'

	} // for event
	processEndFunction();
}//void testanalyser::analyze


void testanalyser::postProcess(){
	/*
	 * This function can be used to analyse the output histograms, e.g. extract a signal contribution etc.
	 * The function can also be called directly on an output file with the histograms, if
	 * RunOnOutputOnly = true
	 * is set in the analyser's config file
	 *
	 * This function also represents an example of how the output of the analyser can be
	 * read-back in an external program.
	 * Just include the sampleCollection.h header and follow the procedure below
	 *
	 */

	/*
	 * Here, the input file to the extraction of parameters from the histograms is the output file
	 * of the parallelised analysis.
	 * The sampleCollection class can also be used externally for accessing the output consistently
	 */
	d_ana::sampleCollection samplecoll;
	samplecoll.readFromFile(getOutPath());

	std::vector<TString> alllegends = samplecoll.listAllLegends();
	
	if(alllegends.size()>0){
		d_ana::histoCollection histos=samplecoll.getHistos(alllegends.at(0));

		/*
		 * here, the histogram created in the analyze() function is selected and evaluated
		 * The histoCollection maintains ownership (you don't need to delete the histogram)
		 */
		const TH1* myplot=histos.getHisto("neutrinoMomentumHisto");

		std::cout << "(example output): the integral is " << myplot->Integral() <<std::endl;
		/*
		 * If the histogram is subject to changes, please clone it and take ownership
		 */

		TH1* myplot2=histos.cloneHisto("neutrinoMomentumHisto");
		
		/*
		 * do something with the histogram
		 */

		delete myplot2;
	}

	/*
	 * do the extraction here.
	 */
}

