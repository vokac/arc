#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "jura.h"

#include <iostream>
//TODO cross-platform
#include <signal.h>
#include <errno.h>
#include <unistd.h>


//TODO cross-platform
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sstream>

#ifdef WIN32
#include <arc/win32.h>
#endif

#include "Reporter.h"
#include "UsageReporter.h"
#include "ReReporter.h"
#include "CARAggregation.h"


int main(int argc, char **argv)
{

  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);

  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::rootLogger.addDestination(logcerr);

  opterr=0;
  time_t ex_period = 0;
  std::vector<std::string> urls;
  std::vector<std::string> topics;
  std::string output_dir;
  bool aggregation  = false;
  bool sync = false;
  bool force_resend = false;
  bool ur_resend = false;
  std::string resend_range = "";
  std::string year  = "";
  std::string month = "";
  std::string vo_filters=""; 
  int n;
  while((n=getopt(argc,argv,":E:u:t:o:y:F:m:r:afsvL")) != -1) {
    switch(n) {
    case ':': { std::cerr<<"Missing argument\n"; return 1; }
    case '?': { std::cerr<<"Unrecognized option\n"; return 1; }
    case 'E': 
      {
        char* p;
        int i = strtol(optarg,&p,10);
        if(((*p) != 0) || (i<=0)) {
          std::cerr<<"Improper expiration period '"<<optarg<<"'"<<std::endl;
          exit(1);
        }
        ex_period=i*(60*60*24);
      } 
      break;
    case 'u':
      urls.push_back(std::string(optarg));
      topics.push_back("");
      break;
    case 't':
      if (topics.begin() == topics.end()){
          std::cerr<<"Add URL value before a topic. (for example: -u [...] -t [...])\n";
          return -1;
      }
      topics.back() = (std::string(optarg));
      break;
    case 'o':
      output_dir = (std::string(optarg));
      break;
    case 'a':
      aggregation = true;
      break;
    case 'y':
      year = (std::string(optarg));
      break;
    case 'F':
      vo_filters = (std::string(optarg));
      break;
    case 'm':
      month = (std::string(optarg));
      break;
    case 'r':
      ur_resend = true;
      resend_range = (std::string(optarg));
      break;
    case 'f':
      std::cout << "Force resend all aggregation records." << std::endl;
      force_resend = true;
      break;
    case 's':
      std::cout << "Sync message(s) will be send..." << std::endl;
      sync = true;
      break;
    case 'v':
      std::cout << Arc::IString("%s version %s", "jura", VERSION)
              << std::endl;
      return 0;
      break;
    case 'L':
      logcerr.setFormat(Arc::LongFormat);
      break;
    default: { std::cerr<<"Options processing error\n"; return 1; }
    }
  }
  
  if ( aggregation ) {
    Arc::CARAggregation* aggr;
    for (int i=0; i<(int)urls.size(); i++)
      {
        std::cout << urls[i] << std::endl;
        //  Tokenize service URL
        std::string host, port, endpoint;
        if (urls[i].empty())
          {
            std::cerr << "ServiceURL missing" << std::endl;
            continue;
          }
        else
          {
            Arc::URL url(urls[i]);
            host=url.Host();
            std::ostringstream os;
            os<<url.Port();
            port=os.str();
            endpoint=url.Path();
          }

        if (topics[i].empty())
          {
            std::cerr << "Topic missing for a (" << urls[i] << ") host." << std::endl;
            continue;
          }
        std::cerr << "Aggregation record(s) sending to " << host << std::endl;
        aggr = new Arc::CARAggregation(host, port, topics[i], sync);

        if ( !year.empty() )
          {
            aggr->Reporting_records(year, month);
          }
        else 
          {
            aggr->Reporting_records(force_resend);
          }
        delete aggr;
      }
    return 0;
  }

  // The essence:
  int argind;
  Arc::Reporter *usagereporter;
  for (argind=optind ; argind<argc ; ++argind)
    {
      if ( ur_resend ) {
          std::cerr << "resend opt:" << resend_range << std::endl;
          usagereporter=new Arc::ReReporter(
                          std::string(argv[argind]),
                          resend_range, urls, topics, vo_filters );

      } else {
          usagereporter=new Arc::UsageReporter(
                          std::string(argv[argind])+"/logs",
                          ex_period, urls, topics, vo_filters, output_dir );
      }
      usagereporter->report();
      delete usagereporter;
    }
  return 0;
}
