
#include <sys/stat.h>
#include <time.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/OptionParser.h>
#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/URL.h>
#include <arc/XMLNode.h>
#include <arc/client/ClientInterface.h>
#include <arc/message/MCCLoader.h>
#include <arc/message/PayloadSOAP.h>

Arc::Logger logger(Arc::Logger::rootLogger, "ISISTest");

// Query function
std::string Query( Arc::URL url, std::string query, Arc::UserConfig usercfg ){
    Arc::MCCConfig mcc_cfg;
    usercfg.ApplyToConfig(mcc_cfg);
    Arc::ClientSOAP client_entry(mcc_cfg, url, usercfg.Timeout());

    // Create and send Query request
    logger.msg(Arc::INFO, "Creating and sending request");
    Arc::NS query_ns;
    query_ns[""] = "http://www.nordugrid.org/schemas/isis/2007/06";
    Arc::PayloadSOAP req(query_ns);
    Arc::XMLNode request = req.NewChild("Query");
    request.NewChild("QueryString") = query;

    std::string request_string;
    request.GetDoc(request_string,true);
    logger.msg(Arc::DEBUG, "Request: %s", request_string);

    Arc::MCC_Status status;
    Arc::PayloadSOAP *resp = NULL;
    std::cout << " Request sent. Waiting for the response." << std::endl;
    status= client_entry.process(&req,&resp);

    if ( (!status.isOk()) || (!resp) || (resp->IsFault()) ) {
      logger.msg(Arc::ERROR, "Request failed");
      std::cerr << "Status: " << std::string(status) << std::endl;
      return "-1";
    };

    std::string response_string;
    (*resp).GetDoc(response_string,true);
    logger.msg(Arc::DEBUG, "Response: %s", response_string);

    //The response message
    std::string response = "";
    if (bool((*resp)["QueryResponse"]) ){
          std::string xml;
          (*resp)["QueryResponse"].GetDoc(xml, true);
          response += xml;
          response +="\n";
    }

    return response;
}


// Register function
std::string Register( Arc::URL url, std::vector<std::string> &serviceID, std::vector<std::string> &epr,
                      std::vector<std::string> type, std::vector<std::string> expiration, Arc::UserConfig usercfg ){

    if (serviceID.size() != epr.size()){
       logger.msg(Arc::VERBOSE, " Service_ID's number is not equivalent with the EPR's number!");
       return "-1";
    }

    Arc::MCCConfig mcc_cfg;
    usercfg.ApplyToConfig(mcc_cfg);
    Arc::ClientSOAP client_entry(mcc_cfg, url, usercfg.Timeout());

    // Create and send Register request
    logger.msg(Arc::INFO, "Creating and sending request");
    Arc::NS query_ns;
    //query_ns[""] = "http://www.nordugrid.org/schemas/isis/2007/06";
    query_ns["wsa"] = "http://www.w3.org/2005/08/addressing";
    Arc::PayloadSOAP req(query_ns);

    Arc::XMLNode request = req.NewChild("Register");
    request.NewChild("Header");
    //request["Header"].NewChild("RequesterID");

    time_t rawtime;
    time ( &rawtime );  //current time
    tm * ptm;
    ptm = gmtime ( &rawtime );

    std::string mon_prefix = (ptm->tm_mon+1 < 10)?"0":"";
    std::string day_prefix = (ptm->tm_mday < 10)?"0":"";
    std::string hour_prefix = (ptm->tm_hour < 10)?"0":"";
    std::string min_prefix = (ptm->tm_min < 10)?"0":"";
    std::string sec_prefix = (ptm->tm_sec < 10)?"0":"";
    std::stringstream out;
    out << ptm->tm_year+1900<<"-"<<mon_prefix<<ptm->tm_mon+1<<"-"<<day_prefix<<ptm->tm_mday<<"T"<<hour_prefix<<ptm->tm_hour<<":"<<min_prefix<<ptm->tm_min<<":"<<sec_prefix<<ptm->tm_sec;
    request["Header"].NewChild("MessageGenerationTime") = out.str();

    for (int i=0; i < serviceID.size(); i++){
        Arc::XMLNode srcAdv = request.NewChild("RegEntry").NewChild("SrcAdv");
        srcAdv.NewChild("Type") = type[i];
        Arc::XMLNode epr_xmlnode = srcAdv.NewChild("EPR");
        epr_xmlnode.NewChild("Address") = epr[i];
        //srcAdv.NewChild("SSPair");

        Arc::XMLNode metaSrcAdv = request["RegEntry"][i].NewChild("MetaSrcAdv");
        metaSrcAdv.NewChild("ServiceID") = serviceID[i];
        metaSrcAdv.NewChild("GenTime") = out.str();
        metaSrcAdv.NewChild("Expiration") = expiration[i];
    }

    std::string request_string;
    request.GetDoc(request_string,true);
    logger.msg(Arc::DEBUG, "Request: %s", request_string);

    Arc::MCC_Status status;
    Arc::PayloadSOAP *resp = NULL;
    std::cout << " Request sent. Waiting for the response." << std::endl;
    status= client_entry.process(&req,&resp);

    if ( (!status.isOk()) || (!resp) || (resp->IsFault()) ) {
      logger.msg(Arc::ERROR, "Request failed");
      std::cerr << "Status: " << std::string(status) << std::endl;
      return "-1";
    };

    std::string response_string;
    (*resp).GetDoc(response_string,true);
    logger.msg(Arc::DEBUG, "Response: %s", response_string);

    //The response message
    std::string response = "";
    if (bool((*resp)["RegisterResponse"]) ){
          std::string xml;
          (*resp)["RegisterResponse"].GetDoc(xml, true);
          response += xml;
          response +="\n";
    }

    return response;
}


// RemoveRegistration function
std::string RemoveRegistration( Arc::URL url, std::vector<std::string> &serviceID, Arc::UserConfig usercfg ){
    Arc::MCCConfig mcc_cfg;
    usercfg.ApplyToConfig(mcc_cfg);
    Arc::ClientSOAP client_entry(mcc_cfg, url, usercfg.Timeout());

    // Create and send RemoveRegistrations request
    logger.msg(Arc::INFO, "Creating and sending request");
    Arc::NS query_ns;
    query_ns[""] = "http://www.nordugrid.org/schemas/isis/2007/06";
    Arc::PayloadSOAP req(query_ns);

    Arc::XMLNode request = req.NewChild("RemoveRegistrations");
    for (std::vector<std::string>::const_iterator it = serviceID.begin(); it != serviceID.end(); it++){
        request.NewChild("ServiceID") = *it;
    }
    time_t rawtime;
    time ( &rawtime );  //current time
    tm * ptm;
    ptm = gmtime ( &rawtime );

    std::string mon_prefix = (ptm->tm_mon+1 < 10)?"0":"";
    std::string day_prefix = (ptm->tm_mday < 10)?"0":"";
    std::string hour_prefix = (ptm->tm_hour < 10)?"0":"";
    std::string min_prefix = (ptm->tm_min < 10)?"0":"";
    std::string sec_prefix = (ptm->tm_sec < 10)?"0":"";
    std::stringstream out;
    out << ptm->tm_year+1900<<"-"<<mon_prefix<<ptm->tm_mon+1<<"-"<<day_prefix<<ptm->tm_mday<<"T"<<hour_prefix<<ptm->tm_hour<<":"<<min_prefix<<ptm->tm_min<<":"<<sec_prefix<<ptm->tm_sec;
    request.NewChild("MessageGenerationTime") = out.str();

    std::string request_string;
    request.GetDoc(request_string,true);
    logger.msg(Arc::DEBUG, "Request: %s", request_string);

    Arc::MCC_Status status;
    Arc::PayloadSOAP *resp = NULL;
    std::cout << " Request sent. Waiting for the response." << std::endl;
    status= client_entry.process(&req,&resp);

    if ( (!status.isOk()) || (!resp) || (resp->IsFault()) ) {
      logger.msg(Arc::ERROR, "Request failed");
      std::cerr << "Status: " << std::string(status) << std::endl;
      return "-1";
    };

    std::string response_string;
    (*resp).GetDoc(response_string,true);
    logger.msg(Arc::DEBUG, "Response: %s", response_string);

    //The response message
    std::string response = "";
    if (bool((*resp)["RemoveRegistrationsResponse"]) ){
          std::string xml;
          (*resp)["RemoveRegistrationsResponse"].GetDoc(xml, true);
          response += xml;
          response +="\n";
    }

    return response;
}

// GetISISList function
std::vector<std::string> GetISISList( Arc::URL url, Arc::UserConfig usercfg ){

    //The response vector
    std::vector<std::string> response;

    Arc::MCCConfig mcc_cfg;
    usercfg.ApplyToConfig(mcc_cfg);
    Arc::ClientSOAP client_entry(mcc_cfg, url, usercfg.Timeout());

    // Create and send GetISISList request
    logger.msg(Arc::INFO, "Creating and sending request");
    Arc::NS query_ns;
    query_ns[""] = "http://www.nordugrid.org/schemas/isis/2007/06";
    Arc::PayloadSOAP req(query_ns);
    Arc::XMLNode request = req.NewChild("GetISISList");

    std::string request_string;
    request.GetDoc(request_string,true);
    logger.msg(Arc::DEBUG, "Request: %s", request_string);

    Arc::MCC_Status status;
    Arc::PayloadSOAP *resp = NULL;
    std::cout << " Request sent. Waiting for the response." << std::endl;
    status= client_entry.process(&req,&resp);

    if ( (!status.isOk()) || (!resp) || (resp->IsFault()) ) {
      logger.msg(Arc::ERROR, "Request failed");
      std::cerr << "Status: " << std::string(status) << std::endl;
      return response;
    };

    std::string response_string;
    (*resp).GetDoc(response_string,true);
    logger.msg(Arc::DEBUG, "Response: %s", response_string);

    // Construct the response vector
    if (bool((*resp)["GetISISListResponse"]) ){
        int i = 0;
        while((bool)(*resp)["GetISISListResponse"]["EPR"][i]) {
            response.push_back((std::string)(*resp)["GetISISListResponse"]["EPR"][i]);
            i++;
        }
    }

    return response;
}

// Split the given string by the given delimiter and return its parts
std::vector<std::string> split( const std::string original_string, const std::string delimiter ) {
    std::vector<std::string> retVal;
    unsigned long start=0;
    unsigned long end;
    while ( ( end = original_string.find( delimiter, start ) ) != std::string::npos ) {
          retVal.push_back( original_string.substr( start, end-start ) );
          start = end + 1;
    }
    retVal.push_back( original_string.substr( start ) );
    return retVal;
}


int main(int argc, char** argv) {

    Arc::LogStream logcerr(std::cerr);
    logcerr.setFormat(Arc::ShortFormat);
    Arc::Logger::getRootLogger().addDestination(logcerr);
    Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

    Arc::OptionParser options(istring("[ISIS testing ...]"),
      istring("This tiny tool can be used for testing "
              "the ISIS's abilities."),
      istring("The method are the folows: Query, Register, RemoveRegistration")
      );

    std::string infosys_url = "";
      options.AddOption('b', "bootstrap",
        istring("define the URL of the Bootstrap ISIS"),
        istring("isis"),
        infosys_url);

    std::string isis_url = "";
      options.AddOption('i', "isis",
        istring("define the URL of the ISIS to connect directly"),
        istring("isis"),
        isis_url);

    std::string method = "";
      options.AddOption('m', "method",
        istring("define which method are use (Query, Register, RemoveRegistration)"),
        istring("method"),
        method);

    std::string debug;
      options.AddOption('d', "debug",
                istring("FATAL, ERROR, WARNING, INFO, VERBOSE or DEBUG"),
                istring("debuglevel"), debug);

    bool neighbors = false;
      options.AddOption('n', "neighbors",
                istring("get neighbors list from the BootstrapISIS"),
                neighbors);

    std::string conffile;
      options.AddOption('z', "conffile",
                istring("configuration file (default ~/.arc/client.conf)"),
                istring("filename"), conffile);

    std::list<std::string> parameters = options.Parse(argc, argv);
    if (!method.empty() && parameters.empty()) {
       std::cout << "Use --help option for detailed usage information" << std::endl;
       return 1;
    }

    if (!debug.empty())
       Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));

    // Create UserConfig
    Arc::UserConfig usercfg(conffile);

    std::cout << " [ ISIS tester ] " << std::endl;
    // Read the first ISIS from the client configuration
    if (isis_url.empty()) {
        Arc::URLListMap ulm = usercfg.GetSelectedServices(Arc::INDEX);
        if (ulm.find("ARC1") != ulm.end()) {
            std::list<Arc::URL> isisFromConfig = ulm.find("ARC1")->second;
            if (!isisFromConfig.empty()){
                isis_url = (*isisFromConfig.begin()).fullstr();
                std::cout << " The following URL from client confifuration will be used: " << isis_url << std::endl;
            }
        }
    }

    if (isis_url.empty() && infosys_url.empty()) {
        std::cout << "ISIS or Bootstrap ISIS have to be defined. Please use -i or -b options. For further options see --help" << std::endl;
        return 1;
    }

    logger.msg(Arc::INFO, " ISIS tester start!");

    std::string contactISIS_address_ = "";

    if (isis_url.empty() || neighbors) {
        if ( !isis_url.empty() && neighbors ) infosys_url = isis_url;
        Arc::URL BootstrapISIS(infosys_url);

        // Get the a list of known ISIS's and choose one from them randomly
        std::vector<std::string> neighbors_ = GetISISList( BootstrapISIS, usercfg );

        std::srand(time(NULL));
        if (neighbors) {
            for (std::vector<std::string>::const_iterator it = neighbors_.begin(); it < neighbors_.end(); it++) {
                std::cout << " Neighbor: " << (*it) << std::endl;
            }
        }
        if (isis_url.empty()) {
            contactISIS_address_ = neighbors_[std::rand() % neighbors_.size()];
            std::cout << " The choosen ISIS list for contact detailed information: " << contactISIS_address_ << std::endl;
        }
        else contactISIS_address_ = isis_url;
    } else {
        contactISIS_address_ = isis_url;
    }
    Arc::URL ContactISIS(contactISIS_address_);
    // end of getISISList

    if (!method.empty()) { std::cout << " [ The selected method:  " << method << " ] " << std::endl; }
    std::string response = "";
    //The method is Query
    if (method == "Query"){
       std::string query_string = "";
       for (std::list<std::string>::const_iterator it=parameters.begin(); it!=parameters.end(); it++){
           query_string += " " + *it;
       }
       response = Query( ContactISIS, *parameters.begin(), usercfg );
       if ( response != "-1" ){
          Arc::XMLNode resp(response);
          Arc::XMLNode queryresponse_;
          resp["Body"]["QueryResponse"].New(queryresponse_);
          queryresponse_.GetDoc(response, true);
          std::cout << " The Query response: " << response << std::endl;
       }
    }
    //The method is Register
    else if (method == "Register"){
       std::vector<std::string> serviceID;
       std::vector<std::string> epr;
       std::vector<std::string> type;
       std::vector<std::string> expiration;

       for (std::list<std::string>::const_iterator it=parameters.begin(); it!=parameters.end(); it++){
           std::vector<std::string> Elements = split( *it, "," );
           if ( Elements.size() >= 2 && Elements.size() <= 4 ) {
              serviceID.push_back(Elements[0]);
              epr.push_back(Elements[1]);
              if ( Elements.size() >= 3 ) type.push_back(Elements[2]);
              else type.push_back("org.nordugrid.tests.isistest");
              if ( Elements.size() >= 4 ) expiration.push_back(Elements[3]);
              else expiration.push_back("PT30M");
           }
           else {
              logger.msg(Arc::ERROR, " Not enough or too much parameters! %s", *it);
              return 1;
           }
       }
       response = Register( ContactISIS, serviceID, epr, type, expiration, usercfg );
       if ( response != "-1" ){
          Arc::XMLNode resp(response);
          if ( bool(resp["Body"]["Fault"]) ){
             std::cout << " The Registeration failed!" << std::endl;
             std::cout << " The fault's name: " << (std::string)resp["Body"]["Fault"]["Name"] << std::endl;
             std::cout << " The fault's type: " << (std::string)resp["Body"]["Fault"]["Type"] << std::endl;
             std::cout << " The fault's description: " << (std::string)resp["Body"]["Fault"]["Description"] << std::endl;
          }
          else {
             std::cout << " The Register method succeeded." << std::endl;
          }
       }
    }
    //The method is RemoveRegistration
    else if (method == "RemoveRegistration"){
       std::vector<std::string> serviceID;

       for (std::list<std::string>::const_iterator it=parameters.begin(); it!=parameters.end(); it++){
              serviceID.push_back(*it);
       }
       response = RemoveRegistration( ContactISIS, serviceID, usercfg );
       if ( response != "-1" ){
          Arc::XMLNode resp(response);
          int i=0;
          Arc::XMLNode responsElements = resp["Body"]["RemoveRegistrationsResponse"]["RemoveRegistrationResponseElement"];
          while ( bool(responsElements[i]) ){
             std::cout << " The RemoveRegistration failed!" << std::endl;
             std::cout << " The ServiceID: " << (std::string)responsElements[i]["ServiceID"] << std::endl;
             std::cout << " The fault's name: " << (std::string)responsElements[i]["Fault"]["Name"] << std::endl;
             std::cout << " The fault's type: " << (std::string)responsElements[i]["Fault"]["Type"] << std::endl;
             std::cout << " The fault's description: " << (std::string)responsElements[i]["Fault"]["Description"] << std::endl;
             i++;
          }

          if (i == 0) {
             std::cout << " The RemoveRegistration method succeeded." << std::endl;
          }
       }
    }
    else {
       std::cout << " [ Method is missing! ] " << std::endl;
       return 0;
    }

    // When any problem is by the SOAP message.
    if ( response == "-1" ){
       std::cout << " No response message or other error in the " << method << " process!" << std::endl;
       return 1;
    }

    return 0;
}

