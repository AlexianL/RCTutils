// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#if !defined(__CLING__) || defined(__ROOTCLING__)
#include <CCDB/BasicCCDBManager.h>
#include <DataFormatsCTP/Configuration.h>
#endif
using namespace o2::ctp;

void GetScalersForRun(int runNumber = 0,int fillN = 0, bool test = 1)
{
  if (test == 0) {
    return;
  }
  std::string mCCDBPathCTPScalers = "CTP/Calib/Scalers";
  std::string mCCDBPathCTPConfig = "CTP/Config/Config";
  //o2::ccdb::CcdbApi api;
  //api.init("http://alice-ccdb.cern.ch"); // alice-ccdb.cern.ch
  //api.init("http://ccdb-test.cern.ch:8080");
  auto& ccdbMgr = o2::ccdb::BasicCCDBManager::instance();
  //ccdbMgr.setURL("http://ccdb-test.cern.ch:8080");
  auto soreor = ccdbMgr.getRunDuration(runNumber);
  uint64_t timeStamp = (soreor.second - soreor.first) /2 + soreor.first;
  std::cout << "Timestamp:" << timeStamp << std::endl;
  //
  std::string sfill = std::to_string(fillN);
  std::map<string,string> metadata;
  metadata["fillNumber"] = sfill;
  auto lhcifdata = ccdbMgr.getSpecific<o2::parameters::GRPLHCIFData>("GLO/Config/GRPLHCIF", timeStamp, metadata);
  auto bfilling = lhcifdata->getBunchFilling();
  std::vector<int> bcs = bfilling.getFilledBCs();
  std::cout << "Number of interacting bc:" << bcs.size() << std::endl;
  //
  std::string srun = std::to_string(runNumber);
  metadata.clear(); // can be empty
  metadata["runNumber"] = srun;
  ccdbMgr.setURL("http://ccdb-test.cern.ch:8080");
  auto ctpscalers = ccdbMgr.getSpecific<CTPRunScalers>(mCCDBPathCTPScalers, timeStamp, metadata);
  if (ctpscalers == nullptr) {
    LOG(info) << "CTPRunScalers not in database, timestamp:" << timeStamp;
  }
  auto ctpcfg = ccdbMgr.getSpecific<CTPConfiguration>(mCCDBPathCTPConfig, timeStamp, metadata);
  if (ctpcfg == nullptr) {
    LOG(info) << "CTPRunConfig not in database, timestamp:" << timeStamp;
  }
  std::cout << "all good" << std::endl;
  ctpscalers->convertRawToO2();
  std::vector<CTPClass> ctpcls = ctpcfg->getCTPClasses();
  int tsc = 255;
  int tce = 255;
  for(auto const& cls: ctpcls) {
    if(cls.name.find("CMTVXTSC-B-NOPF-CRU") != std::string::npos) {
      tsc = cls.getIndex();
      std::cout << cls.name << ":" << tsc << std::endl;
    }
    if(cls.name.find("CMTVXTCE-B-NOPF-CRU") != std::string::npos) {
      tce = cls.getIndex();
      std::cout << cls.name << ":" << tsc << std::endl;
    }
  }
  std::cout << "ZNC:";
  int inp = 26;
  double_t nbc = bcs.size();
  double_t frev = 11245;
  double_t sigmaratio = 28.;
  std::vector<CTPScalerRecordO2> recs = ctpscalers->getScalerRecordO2();
  double_t time0 = recs[0].epochTime;
  double_t timeL = recs[recs.size() - 1].epochTime;
  double_t Trun = timeL-time0;
  std::cout << "time0 = " << time0 << " timeL = " << timeL << " Trun = " << Trun << std::endl;  
  double_t integral = recs[recs.size() - 1].scalersInps[inp - 1] - recs[0].scalersInps[inp - 1];
  double_t rate = integral/Trun;
  double_t rat =  integral/Trun/nbc/frev;
  double_t mu = -TMath::Log(1-rat);
  double_t pp = 1 - mu/(TMath::Exp(mu)-1);
  double_t ratepp = mu*nbc*frev;
  double_t integralpp = ratepp*Trun;
  std::cout << "Rate:"<<rate/sigmaratio << " Integral:" << integral << " mu:" << mu << " Pileup prob:" << pp;
  std::cout << " Integralpp:" << integralpp << " Ratepp:"<< ratepp/sigmaratio <<std::endl;
  //ctpscalers->printInputRateAndIntegral(26);
  //
  std::cout << "TSC:";
  ctpscalers->printClassBRateAndIntegral(tsc+1);
  std::cout << "TCE:";
  ctpscalers->printClassBRateAndIntegral(tce+1);

  // IR at the beginning, the end, and at the middle of the run, integrating over 1 minute
  // each point in the scalers covers 10 seconds
  int nPoints = recs.size();
  int n = recs.size()-1;
  int nIntegratedPoints = 60 / 10 + 1;

  if (nPoints >= nIntegratedPoints) {
    int midStart = nPoints / 2 - nIntegratedPoints / 2;
    int midEnd = midStart + nIntegratedPoints - 1; 
    std::vector<int> startPoint = {0, midStart, nPoints - nIntegratedPoints};
    std::vector<int> endPoint = {nIntegratedPoints - 1, midEnd, nPoints - 1};
    std::vector<string> label = {"start", "mid", "end"};

    for (int ii = 0; ii < startPoint.size(); ++ii) {
      std::cout << "start = " << startPoint[ii] << " end = " << endPoint[ii] << std::endl; 
      double_t tt_znc = (double_t)(recs[endPoint[ii]].intRecord.orbit - recs[startPoint[ii]].intRecord.orbit );
      tt_znc = tt_znc*88e-6;
      Double_t znc = (double_t)(recs[endPoint[ii]].scalersInps[25] - recs[startPoint[ii]].scalersInps[25])/28./tt_znc;

      time0 = recs[startPoint[ii]].epochTime;
      timeL = recs[endPoint[ii]].epochTime;
      Trun = timeL-time0;
      std::cout << "time0 = " << time0 << " timeL = " << timeL << " Trun = " << Trun << std::endl;
      integral = (double_t)(recs[endPoint[ii]].scalersInps[inp - 1] - recs[startPoint[ii]].scalersInps[inp - 1]);
      rate = integral/Trun;
      rat =  integral/Trun/nbc/frev;
      mu = -TMath::Log(1-rat);
      pp = 1 - mu/(TMath::Exp(mu)-1);
      ratepp = mu*nbc*frev;
      integralpp = ratepp*Trun;
      std::cout << label[ii] << ": Rate in the " << label[ii] << " minutes of the run = " << znc << " ratepp_" << label[ii] << " = " << ratepp/sigmaratio << std::endl;
    }
    
  }
  
}
