#ifndef __ARC_AREX_H__
#define __ARC_AREX_H__

#include <arc/infosys/RegisteredService.h>
#include <arc/message/PayloadRaw.h>
#include <arc/delegation/DelegationInterface.h>
#include <arc/infosys/InformationInterface.h>
#include <arc/infosys/InfoRegister.h>

#include "grid-manager/grid_manager.h"

namespace ARex {

class ARexGMConfig;
class ARexConfigContext;

class ARexService: public Arc::RegisteredService {
 protected:
  Arc::NS ns_;
  Arc::Logger logger_;
  Arc::DelegationContainerSOAP delegations_;
  Arc::InformationContainer infodoc_;
  Arc::InfoRegisters inforeg_;
  std::string serviceid_;
  std::string expiration_;
  std::string endpoint_;
  std::string uname_;
  std::string gmconfig_;
  std::string common_name_;
  std::string long_description_;
  std::string lrms_name_;
  std::string os_name_;
  std::string key_path_;
  std::string certificate_path_;
  std::string ca_certificates_dir_;
  std::map<std::string,std::string> glue_states_;
  GridManager* gm_;
  ARexConfigContext* get_configuration(Arc::Message& inmsg);
  Arc::MCC_Status CreateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid);
  Arc::MCC_Status GetActivityStatuses(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status TerminateActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status GetActivityDocuments(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status GetFactoryAttributesDocument(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status StopAcceptingNewActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status StartAcceptingNewActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status ChangeActivityStatus(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status MigrateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid);
  Arc::MCC_Status CacheCheck(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out);
  Arc::MCC_Status make_response(Arc::Message& outmsg);
  Arc::MCC_Status make_fault(Arc::Message& outmsg);
  Arc::MCC_Status make_soap_fault(Arc::Message& outmsg);
  Arc::MCC_Status Get(Arc::Message& inmsg,Arc::Message& outmsg,ARexGMConfig& config,const std::string& id,const std::string& subpath);
  Arc::MCC_Status Put(Arc::Message& inmsg,Arc::Message& outmsg,ARexGMConfig& config,const std::string& id,const std::string& subpath);
  void GenericFault(Arc::SOAPFault& fault);
  void NotAuthorizedFault(Arc::XMLNode fault);
  void NotAuthorizedFault(Arc::SOAPFault& fault);
  void NotAcceptingNewActivitiesFault(Arc::XMLNode fault);
  void NotAcceptingNewActivitiesFault(Arc::SOAPFault& fault);
  void UnsupportedFeatureFault(Arc::XMLNode fault,const std::string& feature);
  void UnsupportedFeatureFault(Arc::SOAPFault& fault,const std::string& feature);
  void CantApplyOperationToCurrentStateFault(Arc::XMLNode fault,const std::string& gm_state,bool failed,const std::string& message);
  void CantApplyOperationToCurrentStateFault(Arc::SOAPFault& fault,const std::string& gm_state,bool failed,const std::string& message);
  void OperationWillBeAppliedEventuallyFault(Arc::XMLNode fault,const std::string& gm_state,bool failed,const std::string& message);
  void OperationWillBeAppliedEventuallyFault(Arc::SOAPFault& fault,const std::string& gm_state,bool failed,const std::string& message);
  void UnknownActivityIdentifierFault(Arc::XMLNode fault,const std::string& message);
  void UnknownActivityIdentifierFault(Arc::SOAPFault& fault,const std::string& message);
  void InvalidRequestMessageFault(Arc::XMLNode fault,const std::string& element,const std::string& message);
  void InvalidRequestMessageFault(Arc::SOAPFault& fault,const std::string& element,const std::string& message);
 public:
  ARexService(Arc::Config *cfg);
  virtual ~ARexService(void);
  virtual Arc::MCC_Status process(Arc::Message& inmsg,Arc::Message& outmsg);
  void InformationCollector(void);
  virtual bool RegistrationCollector(Arc::XMLNode &doc);
  virtual std::string getID();
};

} // namespace ARex

#endif

