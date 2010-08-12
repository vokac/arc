#include <cstdlib>
#include <string>
#include <fstream>

#include <globus_ftp_control.h>
#include <grp.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "userspec.h"
#include "misc/escaped.h"
#include "conf.h"

static Arc::Logger logger(Arc::Logger::getRootLogger(),"userspec_t");

void userspec_t::free(void) {
  // Keep authentication info to preserve proxy (just in case)
}

//userspec_t::userspec_t(void):user(),map(user),default_map(user),name(NULL),group(NULL),home(NULL),gridmap(false) {
userspec_t::userspec_t(void):user(),uid(-1),gid(-1),map(user),default_map(user),gridmap(false) {
}

userspec_t::~userspec_t(void) {
  userspec_t::free();
}

bool check_gridmap(const char* dn,char** user,const char* mapfile) {
  std::string globus_gridmap;
  if(mapfile) {
    globus_gridmap=mapfile;
  }
  else {
    char* tmp=getenv("GRIDMAP");
    if((tmp == NULL) || (tmp[0] == 0)) {
      globus_gridmap="/etc/grid-security/grid-mapfile";
    }
    else { globus_gridmap=tmp; };
  };
  std::ifstream f(globus_gridmap.c_str());
  if(!f.is_open() ) {
    logger.msg(Arc::ERROR, "Mapfile is missing at %s", globus_gridmap);
    return false;
  };
  for(;!f.eof();) {
    std::string buf;//char buf[512]; // must be enough for DN + name
    getline(f,buf);
    //buf[511]=0;
    char* p = &buf[0];
    for(;*p;p++) if(((*p) != ' ') && ((*p) != '\t')) break;
    if((*p) == '#') continue;
    if((*p) == 0) continue;
    std::string val;
    int n = gridftpd::input_escaped_string(p,val);
    if(strcmp(val.c_str(),dn) != 0) continue;
    p+=n;
    if(user) {
      n=gridftpd::input_escaped_string(p,val);
      *user=strdup(val.c_str());
    };
    f.close();
    return true;
  };
  f.close();
  return false;
}

/*
int fill_user_spec(userspec_t *spec,globus_ftp_control_auth_info_t *auth,globus_ftp_control_handle_t *handle) {
  if(spec == NULL) return 1;
  if(auth == NULL) return 1;
  if(!(spec->fill(auth,handle)
    )) return 1;
  return 0;
}
*/

bool userspec_t::fill(globus_ftp_control_auth_info_t *auth,globus_ftp_control_handle_t *handle) {
  struct passwd pw_;
  struct group gr_;
  struct passwd *pw;
  struct group *gr;
  char buf[BUFSIZ];
  if(auth == NULL) return false;
  if(auth->auth_gssapi_subject == NULL) return false;
  std::string subject = auth->auth_gssapi_subject;
  gridftpd::make_unescaped_string(subject);
  char* name=NULL;
  if(!check_gridmap(subject.c_str(),&name)) {
    logger.msg(Arc::WARNING, "There is no local mapping for user");
    name=NULL;
  } else {
    if((name == NULL) || (name[0] == 0)) {
      logger.msg(Arc::WARNING, "There is no local name for user");
      if(name) { std::free(name); name=NULL; };
    } else {
      gridmap=true;
    };
  };
  // fill host info
  if(handle) {
    //int host[4] = {0,0,0,0};
    //unsigned short port = 0;
    if(globus_io_tcp_get_remote_address(&(handle->cc_handle.io_handle),
           host,&(port)) != GLOBUS_SUCCESS) {
      port=0;
      user.set(auth->auth_gssapi_subject,auth->auth_gssapi_context,
               auth->delegated_credential_handle);
    } else {
      char abuf[1024]; abuf[sizeof(abuf)-1]=0;
      struct hostent he;
      struct hostent* he_p;
      struct in_addr a;
      snprintf(abuf,sizeof(abuf)-1,"%u.%u.%u.%u",host[0],host[1],host[2],host[3]);
      if(inet_aton(abuf,&a) != 0) {
        int h_errnop;
        char buf[1024];
        he_p=globus_libc_gethostbyaddr_r((char*)&a,strlen(abuf),AF_INET,
                                               &he,buf,sizeof(buf),&h_errnop);
        if(he_p) {
          if(strcmp(he_p->h_name,"localhost") == 0) {
            abuf[sizeof(abuf)-1]=0;
            if(globus_libc_gethostname(abuf,sizeof(abuf)-1) != 0) {
              strcpy(abuf,"localhost");
            };
          };
        };
      };
      user.set(auth->auth_gssapi_subject,auth->auth_gssapi_context,
               auth->delegated_credential_handle,abuf);
    };
  };
  if((!user.is_proxy()) || (user.proxy() == NULL) || (user.proxy()[0] == 0)) {
    logger.msg(Arc::INFO, "No proxy provided");
  } else {
    logger.msg(Arc::VERBOSE, "Proxy stored at %s", user.proxy());
  };
  if((getuid() == 0) && name) {
    logger.msg(Arc::INFO, "Initially mapped to local user: %s", name);
    getpwnam_r(name,&pw_,buf,BUFSIZ,&pw);
    if(pw == NULL) {
      logger.msg(Arc::ERROR, "Local user does not exist");
      std::free(name); name=NULL;
      return false;
    };
  } else {
    if(name) std::free(name); name=NULL;
    getpwuid_r(getuid(),&pw_,buf,BUFSIZ,&pw);
    if(pw == NULL) {
      logger.msg(Arc::WARNING, "Running user has no name");
    } else {
      name=strdup(pw->pw_name);
      logger.msg(Arc::INFO, "Mapped to running user: %s", name);
    };
  };
  if(pw) {
    uid=pw->pw_uid;
    gid=pw->pw_gid;
    logger.msg(Arc::VERBOSE, "Mapped to local id: %i", pw->pw_uid);
    home=pw->pw_dir;
    getgrgid_r(pw->pw_gid,&gr_,buf,BUFSIZ,&gr);
    if(gr == NULL) {
      logger.msg(Arc::INFO, "No group %i for mapped user", gid);
    };
    std::string mapstr;
    if(name) mapstr+=name;
    mapstr+=":";
    if(gr) mapstr+=gr->gr_name;
    mapstr+=" all";
    default_map.mapname(mapstr.c_str());
    logger.msg(Arc::VERBOSE, "Mapped to local group id: %i", pw->pw_gid);
    if(gr) logger.msg(Arc::VERBOSE, "Mapped to local group name: %s", gr->gr_name);
    logger.msg(Arc::VERBOSE, "Mapped user's home: %s", home);
  };
  if(name) std::free(name);
  return true;
}

bool userspec_t::fill(AuthUser& u) {
  struct passwd pw_;
  struct group gr_;
  struct passwd *pw;
  struct group *gr;
  char buf[BUFSIZ];
  std::string subject = u.DN();
  char* name=NULL;
  if(!check_gridmap(subject.c_str(),&name)) {
    logger.msg(Arc::WARNING, "There is no local mapping for user");
    name=NULL;
  } else {
    if((name == NULL) || (name[0] == 0)) {
      logger.msg(Arc::WARNING, "There is no local name for user");
      if(name) { std::free(name); name=NULL; };
    } else {
      gridmap=true;
    };
  };
  user=u;
  if((!user.is_proxy()) || (user.proxy() == NULL) || (user.proxy()[0] == 0)) {
    logger.msg(Arc::INFO, "No proxy provided");
  } else {
    logger.msg(Arc::INFO, "Proxy stored at %s", user.proxy());
  };
  if((getuid() == 0) && name) {
    logger.msg(Arc::INFO, "Initially mapped to local user: %s", name);
    getpwnam_r(name,&pw_,buf,BUFSIZ,&pw);
    if(pw == NULL) {
      logger.msg(Arc::ERROR, "Local user does not exist");
      std::free(name); name=NULL;
      return false;
    };
  } else {
    if(name) std::free(name); name=NULL;
    getpwuid_r(getuid(),&pw_,buf,BUFSIZ,&pw);
    if(pw == NULL) {
      logger.msg(Arc::WARNING, "Running user has no name");
    } else {
      name=strdup(pw->pw_name);
      logger.msg(Arc::INFO, "Mapped to running user: %s", name);
    };
  };
  if(pw) {
    uid=pw->pw_uid;
    gid=pw->pw_gid;
    logger.msg(Arc::INFO, "Mapped to local id: %i", pw->pw_uid);
    home=pw->pw_dir;
    getgrgid_r(pw->pw_gid,&gr_,buf,BUFSIZ,&gr);
    if(gr == NULL) {
      logger.msg(Arc::INFO, "No group %i for mapped user", gid);
    };
    std::string mapstr;
    if(name) mapstr+=name;
    mapstr+=":";
    if(gr) mapstr+=gr->gr_name;
    mapstr+=" all";
    default_map.mapname(mapstr.c_str());
    logger.msg(Arc::INFO, "Mapped to local group id: %i", pw->pw_gid);
    if(gr) logger.msg(Arc::INFO, "Mapped to local group name: %s", gr->gr_name);
    logger.msg(Arc::INFO, "Mapped user's home: %s", home);
  };
  if(name) std::free(name);
  return true;
}

std::string subst_user_spec(std::string &in,userspec_t *spec) {
  std::string out = "";
  unsigned int i;
  unsigned int last;
  last=0;
  for(i=0;i<in.length();i++) {
    if(in[i] == '%') {
      if(i>last) out+=in.substr(last,i-last);
      i++; if(i>=in.length()) {         };
      switch(in[i]) {
        case 'u': {
          char buf[10];
          snprintf(buf,9,"%i",spec->uid); out+=buf; last=i+1; 
        }; break;
        case 'U': { out+=spec->get_uname(); last=i+1; }; break;
        case 'g': {
          char buf[10];
          snprintf(buf,9,"%i",spec->gid); out+=buf; last=i+1; 
        }; break;
        case 'G': { out+=spec->get_gname(); last=i+1; }; break;
        case 'D': { out+=spec->user.DN(); last=i+1; }; break;
        case 'H': { out+=spec->home; last=i+1; }; break;
        case '%': { out+='%'; last=i+1; }; break;
        default: {
          logger.msg(Arc::WARNING, "Undefined control sequence: %%%s", in[i]);
        };
      };
    };
  };
  if(i>last) out+=in.substr(last);
  return out;
}

bool userspec_t::refresh(void) {
  if(!map) return false;
  home=""; uid=-1; gid=-1; 
  const char* name = map.unix_name();
  const char* group = map.unix_group();
  if(name == NULL) return false;
  if(name[0] == 0) return false;
  struct passwd pw_;
  struct group gr_;
  struct passwd *pw;
  struct group *gr;
  char buf[BUFSIZ];
  getpwnam_r(name,&pw_,buf,BUFSIZ,&pw);
  if(pw == NULL) {
    logger.msg(Arc::ERROR, "Local user %s does not exist", name);
    return false;
  };
  uid=pw->pw_uid;
  home=pw->pw_dir;
  gid=pw->pw_gid;
  if(group && group[0]) {
    getgrnam_r(group,&gr_,buf,BUFSIZ,&gr);
    if(gr == NULL) {
      logger.msg(Arc::WARNING, "Local group %s does not exist", group);
    } else {
      gid=gr->gr_gid;
    };
  };
  logger.msg(Arc::INFO, "Remapped to local user: %s", name);
  logger.msg(Arc::INFO, "Remapped to local id: %i", uid);
  logger.msg(Arc::INFO, "Remapped to local group id: %i", gid);
  if(group && group[0]) logger.msg(Arc::INFO, "Remapped to local group name: %s", group);
  logger.msg(Arc::INFO, "Remapped user's home: %s", home);
  return true;
}

bool userspec_t::mapname(const char* line) {
  if(!map.mapname(line)) return false;
  refresh();
  return true;
}

bool userspec_t::mapgroup(const char* line) {
  if(!map.mapgroup(line)) return false;
  refresh();
  return true;
}

bool userspec_t::mapvo(const char* line) {
  if(!map.mapvo(line)) return false;
  refresh();
  return true;
}

const char* userspec_t::get_uname(void) {
  const char* name = NULL;
  if((bool)map) { name=map.unix_name(); }
  else if((bool)default_map) { name=default_map.unix_name(); };
  if(!name) name="";
  return name;
}

const char* userspec_t::get_gname(void) {
  const char* group = NULL;
  if((bool)map) { group=map.unix_group(); }
  else if((bool)default_map) { group=default_map.unix_group(); };
  if(!group) group="";
  return group;
}

