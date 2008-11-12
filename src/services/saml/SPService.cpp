#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <fstream>

#include <arc/DateTime.h>
#include <arc/GUID.h>
#include <arc/URL.h>
#include <arc/XMLNode.h>
#include <arc/loader/Loader.h>
#include <arc/loader/ServiceLoader.h>
#include <arc/message/PayloadRaw.h>
#include <arc/security/SecHandler.h>
#include <arc/credential/Credential.h>
#include <arc/client/ClientInterface.h>

#include <arc/xmlsec/XmlSecUtils.h>
#include <arc/xmlsec/XMLSecNode.h>
#include <arc/xmlsec/saml_util.h>

#include "SPService.h"

static Arc::Service* get_service(Arc::Config *cfg,Arc::ChainContext*) {
    return new SPService::Service_SP(cfg);
}

service_descriptors ARC_SERVICE_LOADER = {
    { "saml.sp", 0, &get_service },
    { NULL, 0, NULL }
};

namespace SPService {

Service_SP::Service_SP(Arc::Config *cfg):Service(cfg),logger(Arc::Logger::rootLogger, "SAML2SP") {
  std::string metadata_file = "test_metadata.xml";
  metadata_node_.ReadFromFile(metadata_file);
  std::string str;
  metadata_node_.GetXML(str);
  //std::cout<<"Metadata: "<<str<<std::endl;
  if(!(Arc::init_xmlsec())) return;
}

Service_SP::~Service_SP(void) {
  Arc::final_xmlsec();
  std::ofstream file_authn_record("auth_record", std::ios_base::trunc);
  file_authn_record.close();
}

Arc::MCC_Status Service_SP::process(Arc::Message& inmsg,Arc::Message& outmsg) {

  // Check authentication and authorization
  if(!ProcessSecHandlers(inmsg, "incoming")) {
    logger.msg(Arc::ERROR, "saml2SP: Unauthorized");
    return Arc::MCC_Status(Arc::GENERIC_ERROR);
  };
  // Both input and output are supposed to be HTTP 
  // Extracting payload
  Arc::PayloadRawInterface* inpayload = dynamic_cast<Arc::PayloadRawInterface*>(inmsg.Payload());
  if(!inpayload) {
    logger.msg(Arc::WARNING, "empty input payload");
  };

  //Analyzing http request from user agent
  //for(Arc::AttributeIterator i = inmsg.Attributes()->getAll();i.hasMore();++i) {
  //  std::cout<<"Attribute: "<<i.key()<<"=="<<*i<<std::endl;
  //}

  std::string msg_content(inpayload->Content());
  //SP service is supposed to get two types of http content from user agent:
  //1. The IdP name, which SP uses to generate AuthnRequest
  //2. The saml assertion, which user agent gets from IdP 
  
  if(msg_content.substr(0,4) == "http") {
  //Get the IdP name from the request
  //Here we require the user agent to provide the idp name instead of the 
  //WRYF(where are you from) or Discovery Service in some other implementation
  //like Shibboleth
  std::string idp_name(msg_content);
  //Compose <samlp:AuthnRequest/>
  Arc::NS saml_ns;
  saml_ns["saml"] = SAML_NAMESPACE;
  saml_ns["samlp"] = SAMLP_NAMESPACE;
  Arc::XMLNode authn_request(saml_ns, "samlp:AuthnRequest");
  std::string sp_name("https://squark.uio.no/shibboleth-sp"); //TODO
  std::string req_id = Arc::UUID();
  authn_request.NewAttribute("ID") = req_id;
  Arc::Time t1;
  std::string current_time1 = t1.str(Arc::UTCTime);
  authn_request.NewAttribute("IssueInstant") = current_time1;
  authn_request.NewAttribute("Version") = std::string("2.0");

  //Get url of assertion consumer service from metadata
  std::string assertion_consumer_url;
  for(int i = 0;;i++) {
    Arc::XMLNode nd = metadata_node_.Child(i);
    if(!nd) break;
    if(sp_name == (std::string)(nd.Attribute("entityID"))) {
      for(int j = 0;; j++) {
        Arc::XMLNode sp_nd = nd.Child(j);
        if(!sp_nd) break;
        if(MatchXMLName(sp_nd,"SPSSODescriptor")) {
          for(int k = 0;;k++) {
            Arc::XMLNode assertionconsumer_nd = sp_nd.Child(k);
            if(!assertionconsumer_nd) break;        
            if(MatchXMLName(assertionconsumer_nd, "AssertionConsumerService")) {
              if((std::string)(assertionconsumer_nd.Attribute("Binding")) == "urn:oasis:names:tc:SAML:2.0:bindings:HTTP-POST")
                assertion_consumer_url = (std::string)(assertionconsumer_nd.Attribute("Location"));
            }
            if(!assertion_consumer_url.empty()) break;
          }
        }
        if(!assertion_consumer_url.empty()) break;
      }
    }
  }
  authn_request.NewAttribute("AssertionConsumerServiceURL") = assertion_consumer_url;

  //Get url of sso service from metadata
  std::string sso_url;
  for(int i = 0;;i++) {
    Arc::XMLNode nd = metadata_node_.Child(i);
    if(!nd) break;
    if(idp_name == (std::string)(nd.Attribute("entityID"))) {
      for(int j = 0;; j++) {
        Arc::XMLNode idp_nd = nd.Child(j);
        if(!idp_nd) break;
        if(MatchXMLName(idp_nd,"IDPSSODescriptor")) {
          for(int k = 0;;k++) {
            Arc::XMLNode sso_nd = idp_nd.Child(k);
            if(!sso_nd) break;
            if(MatchXMLName(sso_nd, "SingleSignOnService")) {
              if((std::string)(sso_nd.Attribute("Binding")) == "urn:oasis:names:tc:SAML:2.0:bindings:HTTP-Redirect")
                sso_url = (std::string)(sso_nd.Attribute("Location"));
            }
            if(!sso_url.empty()) break;
          }
        }
        if(!sso_url.empty()) break;
      }
    }
  }
  authn_request.NewAttribute("Destination") = sso_url;
  authn_request.NewAttribute("ProtocolBinding") = std::string("urn:oasis:names:tc:SAML:2.0:bindings:HTTP-POST");
  authn_request.NewChild("saml:Issuer") = sp_name;

  Arc::XMLNode nameid_policy = authn_request.NewChild("samlp:NameIDPolicy");
  nameid_policy.NewAttribute("AllowCreate") = std::string("1");

  bool must_signed = false; //TODO: get the information from metadata

  std::string cert_file = "testcert.pem";  //TODO: another configuration file?
  std::string privkey_file = "testkey-nopass.pem";

  std::string authnRequestQuery;
  std::string query = BuildDeflatedQuery(authn_request);
  //std::cout<<"AuthnRequest after deflation: "<<query<<std::endl;
  if(must_signed) {
    authnRequestQuery = SignQuery(query, Arc::RSA_SHA1, privkey_file);
    //std::cout<<"After signature: "<<authnRequestQuery<<std::endl;
  }
  else authnRequestQuery = query;

  std::string authnRequestUrl;
  authnRequestUrl = sso_url + "?SAMLRequest=" + authnRequestQuery;

  //Return the composed url back to user agent through http
  Arc::PayloadRaw* outpayload = NULL;
  outpayload = new Arc::PayloadRaw;
  outpayload->Insert(authnRequestUrl.c_str(),0, authnRequestUrl.size());
  outmsg.Attributes()->set("HTTP:CODE","302");
  outmsg.Attributes()->set("HTTP:REASON","Moved Temporarily");
  delete outmsg.Payload(outpayload);
  }
  else {  
    //The http content should be <saml:EncryptedAssertion/> or <saml:Assertion/>
    //Decrypted the assertion (if it is encrypted) by using SP's key (the key is the 
    //same as the one for the main message chain)
    //std::cout<<"saml assertion from peer side: "<<msg_content<<std::endl;
    Arc::XMLNode assertion_nd(msg_content);
    if(MatchXMLName(assertion_nd, "EncryptedAssertion")) {
      //Decrypte the encrypted saml assertion
      std::string saml_assertion;
      assertion_nd.GetXML(saml_assertion);
      //std::cout<<"Encrypted saml assertion: "<<saml_assertion<<std::endl;

      Arc::XMLSecNode sec_assertion_nd(assertion_nd);
      Arc::XMLNode decrypted_assertion_nd;

      std::string privkey_file_ = "testkey-nopass.pem";

      bool r = sec_assertion_nd.DecryptNode(privkey_file_, decrypted_assertion_nd);
      if(!r) { 
        logger.msg(Arc::ERROR,"Can not decrypted the EncryptedAssertion from saml response"); 
        return Arc::MCC_Status(); 
      }

      std::string decrypted_saml_assertion;
      decrypted_assertion_nd.GetXML(decrypted_saml_assertion);
      //std::cout<<"Decrypted SAML Assertion: "<<decrypted_saml_assertion<<std::endl;
     
      //Decrypted the <saml:EncryptedID/> if it exists in the above saml assertion
      Arc::XMLNode nameid_nd = decrypted_assertion_nd["saml:Subject"]["saml:EncryptedID"];
      std::string nameid;
      nameid_nd.GetXML(nameid);
      //std::cout<<"Encrypted name id: "<<nameid<<std::endl;

      Arc::XMLSecNode sec_nameid_nd(nameid_nd);
      Arc::XMLNode decrypted_nameid_nd;
      r = sec_nameid_nd.DecryptNode(privkey_file_, decrypted_nameid_nd);
      if(!r) { logger.msg(Arc::ERROR,"Can not decrypted the EncryptedID from saml assertion"); return Arc::MCC_Status(); }

      std::string decrypted_nameid;
      decrypted_nameid_nd.GetXML(decrypted_nameid);
      //std::cout<<"Decrypted SAML NameID: "<<decrypted_nameid<<std::endl;

      //TODO: Check the authentication part of saml assertion

      //Record the authentication, this information will be checked by 
      //saml2sso_serviceprovider handler later to decide whether pass
      //the incoming message which is from the same session as the saml2sso
      //process
      std::string authn_record;
      authn_record.append(inmsg.Attributes()->get("TCP:REMOTEHOST")).append(":")
                  .append(inmsg.Attributes()->get("TCP:REMOTEPORT")).append("\n");
      std::ofstream file_authn_record("auth_record", std::ios_base::app);
      file_authn_record.write(authn_record.c_str(),authn_record.size());
      file_authn_record.close();

      Arc::PayloadRaw* outpayload = NULL;
      outpayload = new Arc::PayloadRaw;
      //outpayload->Insert(authnRequestUrl.c_str(),0, authnRequestUrl.size());
      //outmsg.Attributes()->set("HTTP:CODE","302");
      //outmsg.Attributes()->set("HTTP:REASON","Moved Temporarily");
      delete outmsg.Payload(outpayload);
    }
    else if(MatchXMLName(assertion_nd, "Assertion")) {

    }
    else {

    }

  }

  return Arc::MCC_Status(Arc::STATUS_OK);
}

} // namespace SPService

