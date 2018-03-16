// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
#include <Rmath.h>
#include <thread>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <climits>
extern "C" {
#include "solve.h"
}
#define rxModelVars(a) rxModelVars_(a)
using namespace Rcpp;
using namespace arma;

int rxcEvid = -1;
int rxcTime = -1;
int rxcAmt  = -1;
int rxcId   = -1;
int rxcDv   = -1;
int rxcLen  = -1;
bool resetCache = true;
bool rxHasEventNames(CharacterVector &nm){
  int len = nm.size();
  bool reset  = resetCache;
  if (reset || len != rxcLen){
    reset   = resetCache;
    rxcEvid = -1;
    rxcTime = -1;
    rxcAmt  = -1;
    rxcId   = -1;
    rxcDv   = -1;
    rxcLen  = len;
    for (unsigned int i = len; i--;){
      if (as<std::string>(nm[i]) == "evid" || as<std::string>(nm[i]) == "EVID" || as<std::string>(nm[i]) == "Evid"){
        rxcEvid = i;
      } else if (as<std::string>(nm[i]) == "time" || as<std::string>(nm[i]) == "TIME" || as<std::string>(nm[i]) == "Time"){
        rxcTime = i;
      } else if (as<std::string>(nm[i]) == "amt" || as<std::string>(nm[i]) == "AMT" || as<std::string>(nm[i]) == "Amt"){
        rxcAmt = i;
      } else if (as<std::string>(nm[i]) == "id" || as<std::string>(nm[i]) == "ID" || as<std::string>(nm[i]) == "Id"){
        rxcId = i;
      } else if (as<std::string>(nm[i]) == "dv" || as<std::string>(nm[i]) == "DV" || as<std::string>(nm[i]) == "Dv"){
        rxcDv = i;
      }
    }
  }
  resetCache = true;
  if (rxcEvid >= 0 && rxcTime >= 0 && rxcAmt >= 0){
    return true;
  } else {
    return false;
  }
}

//' Check the type of an object using Rcpp
//'
//' @param obj Object to check
//' @param cls Type of class.  Only s3 classes for lists/environments and primitive classes are checked.
//'    For matrix types they are distinguished as \code{numeric.matrix}, \code{integer.matrix},
//'    \code{logical.matrix}, and \code{character.matrix} as well as the traditional \code{matrix}
//'    class. Additionally checks for \code{event.data.frame} which is an \code{data.frame} object
//'    with \code{time},  \code{evid} and \code{amt}. (UPPER, lower or Title cases accepted)
//'
//' @return A boolean indicating if the object is a member of the class.
//' @keywords internal
//' @author Matthew L. Fidler
//' @export
// [[Rcpp::export]]
bool rxIs(const RObject &obj, std::string cls){
  int type = obj.sexp_type();
  bool hasDim = false;
  bool hasCls = false;
  switch (type){
  case 0: return (cls == "NULL");
  case REALSXP: 
    hasDim = obj.hasAttribute("dim");
    if (hasDim){
      if (cls == "event.matrix" || cls ==  "rx.event"){
	if (obj.hasAttribute("dimnames")){
	  List dn = as<List>(obj.attr("dimnames"));
          if (dn.size() == 2){
            CharacterVector cv = as<CharacterVector>(dn[1]);
            return rxHasEventNames(cv);
          } else {
	    return false;
	  }
	} else {
	  return false;
	}
      } else {
	return (cls == "matrix" || cls == "numeric.matrix");
      }
    } else {
      return (cls == "numeric");
    }
  case 13: // integer vectors
    // An integer vector cannot be an event matrix.
    hasDim = obj.hasAttribute("dim");
    if (hasDim){
      return (cls == "matrix" || cls == "integer.matrix");
    } else {
      return (cls == "integer");
    }
  case LGLSXP:
    hasDim = obj.hasAttribute("dim");
    if (hasDim){
      return (cls == "matrix" ||  cls == "logical.matrix");
    } else {
      return (cls == "logical");
    }
  case STRSXP:
    hasDim = obj.hasAttribute("dim");
    if (hasDim){
      return (cls == "matrix" || cls == "character.matrix");
    } else {
      return (cls == "character");
    }
  case VECSXP:
    hasCls = obj.hasAttribute("class");
    if (hasCls){
      CharacterVector classattr = obj.attr("class");
      bool hasDf = false;
      bool hasEt = false;
      std::string cur;
      for (unsigned int i = classattr.size(); i--; ){
	cur = as<std::string>(classattr[i]);
	if (cur == cls){
	  if (cls == "rxSolve"){
	    Environment e = as<Environment>(classattr.attr(".RxODE.env"));
	    List lobj = List(obj);
	    CharacterVector cls2= CharacterVector::create("data.frame");
	    if (as<int>(e["check.ncol"]) != lobj.size()){
	      lobj.attr("class") = cls2;
	      return false;
	    }
	    int nrow = (as<NumericVector>(lobj[0])).size();
	    if (as<int>(e["check.nrow"]) != nrow){
	      lobj.attr("class") = cls2;
	      return false;
	    }
	    CharacterVector cn = CharacterVector(e["check.names"]);
	    if (cn.size() != lobj.size()){
	      lobj.attr("class") = cls2;
	      return false;
	    }
	    CharacterVector cn2 = CharacterVector(lobj.names());
	    for (int j = 0; j < cn.size();j++){
	      if (cn[j] != cn2[j]){
		lobj.attr("class") = cls2;
		return false;
	      }
	    }
	    return true;
	  } else {
	    return true;
	  }
	} else if (cur == "data.frame"){
	  hasDf=true;
        } else if (cur == "EventTable"){
	  hasEt=true;
	}
      }
      if (hasDf && (cls == "rx.event" || cls == "event.data.frame")){
	// Check for event.data.frame
	CharacterVector cv =as<CharacterVector>((as<DataFrame>(obj)).names());
	return rxHasEventNames(cv);
      } else if (hasEt) {
	return (cls == "rx.event");
      } else {
	return false;
      }
    } else {
      return (cls == "list");
    }
  case 4: // environment
    hasCls = obj.hasAttribute("class");
    if (hasCls){
      CharacterVector classattr = obj.attr("class");
      std::string cur;
      for (unsigned int i = classattr.size(); i--; ){
        cur = as<std::string>(classattr[i]);
	if (cur == cls) return true;
      }
    } else if (cls == "environment"){
       return true;
    }
    return false;
  case 22: // external pointer
    return (cls == "externalptr" || cls == "refObject");
  }
  return false;
}

RObject rxSimSigma(const RObject &sigma,
		   const RObject &df,
		   int ncores,
		   const bool &isChol,
		   int nObs,
		   const bool checkNames = true){
  if (rxIs(sigma, "numeric.matrix")){
    // FIXME more distributions
    NumericMatrix sigmaM(sigma);
    if (sigmaM.nrow() != sigmaM.ncol()){
      stop("The matrix must be a square matrix.");
    }
    List dimnames;
    StringVector simNames;
    bool addNames = false;
    if (checkNames){
      if (!sigmaM.hasAttribute("dimnames")){
        stop("The matrix must have named dimensions.");
      }
      dimnames = sigmaM.attr("dimnames");
      simNames = as<StringVector>(dimnames[1]);
      addNames = true;
    } else if (sigmaM.hasAttribute("dimnames")){
      dimnames = sigmaM.attr("dimnames");
      simNames = as<StringVector>(dimnames[1]);
      addNames = true;
    }
    Environment base("package:base");
    Function loadNamespace=base["loadNamespace"];
    Environment mvnfast = loadNamespace("mvnfast");
    NumericMatrix simMat(nObs,sigmaM.ncol());
    NumericVector m(sigmaM.ncol());
    // I'm unsure if this for loop is necessary.
    // for (int i = 0; i < m.size(); i++){
    //   m[i] = 0;
    // }
    // Ncores = 1?  Should it be parallelized when it can be...?
    // Note that if so, the number of cores also affects the output.
    if (df.isNULL()){
      Function rmvn = as<Function>(mvnfast["rmvn"]);
      rmvn(_["n"]=nObs, _["mu"]=m, _["sigma"]=sigmaM, _["ncores"]=ncores, _["isChol"]=isChol, _["A"] = simMat); // simMat is updated with the random deviates
    } else {
      double df2 = as<double>(df);
      if (R_FINITE(df2)){
	Function rmvt = as<Function>(mvnfast["rmvt"]);
        rmvt(_["n"]=nObs, _["mu"]=m, _["sigma"]=sigmaM, _["df"] = df, _["ncores"]=ncores, _["isChol"]=isChol, _["A"] = simMat);
      } else {
	Function rmvn = as<Function>(mvnfast["rmvn"]);
        rmvn(_["n"]=nObs, _["mu"]=m, _["sigma"]=sigmaM, _["ncores"]=ncores, _["isChol"]=isChol, _["A"] = simMat);
      }
    }
    if (addNames){
      simMat.attr("dimnames") = List::create(R_NilValue, simNames);
    }
    return wrap(simMat);
  } else {
    return R_NilValue;
  }
}

bool foundEnv = false;
Environment _rxModels;
void getRxModels(){
  if (!foundEnv){ // minimize R call
    Environment RxODE("package:RxODE");
    Function f = as<Function>(RxODE["rxModels_"]);
    _rxModels = f();
    foundEnv = true;
  }
}

// 
inline bool fileExists(const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

// [[Rcpp::export]]
List rxModelVars_(const RObject &obj){
  getRxModels();
  if (rxIs(obj, "rxModelVars")){
    List ret(obj);
    return ret;
  } else if (rxIs(obj,"RxODE")) {
    Environment e = as<Environment>(obj);
    List rxDll = e["rxDll"];
    List ret = rxDll["modVars"];
    return ret;
  } else if (rxIs(obj,"rxSolve")){
    CharacterVector cls = obj.attr("class");
    Environment e = as<Environment>(cls.attr(".RxODE.env"));
    return  rxModelVars_(as<RObject>(e["args.object"]));
  } else if (rxIs(obj,"rxDll")){
    List lobj = (as<List>(obj))["modVars"];
    return lobj;
  } else if (rxIs(obj, "character")){
    CharacterVector modList = as<CharacterVector>(obj);
    if (modList.size() == 1){
      std::string sobj =as<std::string>(obj);
      if ((sobj.find("=") == std::string::npos) &&
	  (sobj.find("<-") == std::string::npos)){
        if (_rxModels.exists(sobj)){
          RObject obj1 = _rxModels.get(sobj);
          if (rxIs(obj1, "rxModelVars")){
            return as<List>(obj1);
          } else if (rxIs(obj1, "RxODE")){
            return rxModelVars_(obj1);
          }
        }
        std::string sobj1 = sobj + "_model_vars";
        if (_rxModels.exists(sobj1)){
          RObject obj1 = _rxModels.get(sobj1);
          if (rxIs(obj1, "rxModelVars")){
            return as<List>(obj1);
          }
        }
        Function get("get",R_BaseNamespace);
        List platform = get(_["x"]=".Platform", _["envir"] = R_BaseEnv);
        sobj1 = sobj + "_" + as<std::string>(platform["r_arch"]) + "_model_vars";
        if (_rxModels.exists(sobj1)){
          RObject obj1 = _rxModels.get(sobj1);
          if (rxIs(obj1, "rxModelVars")){
            return as<List>(obj1);
          }
        }
        Function filePath("file.path", R_BaseNamespace);
        Function getwd("getwd", R_BaseNamespace);
        sobj1 = as<std::string>(getwd());
        std::string sobj2 = sobj + ".d";
        std::string sobj3 = sobj + "_" + as<std::string>(platform["r_arch"]) +
          as<std::string>(platform["dynlib.ext"]);
        sobj1 = as<std::string>(filePath(sobj1,sobj2, sobj3));
        if (fileExists(sobj1)){
          Rcout << "Path: " << sobj1 << "\n";
          Function dynLoad("dyn.load", R_BaseNamespace);
          dynLoad(sobj1);
          sobj1 = sobj + "_" + as<std::string>(platform["r_arch"]) +
            "_model_vars";
          Function call(".Call", R_BaseNamespace);
          List ret = as<List>(call(sobj1));
          return ret;
        }
      }
    } else if (modList.hasAttribute("names")){
      bool containsPrefix = false;
      CharacterVector modListNames = modList.names();
      for (int i = 0; i < modListNames.size(); i++){
	if (modListNames[i] == "prefix"){
	  containsPrefix=true;
	  break;
	}
      }
      if (containsPrefix){
	std::string mvstr = as<std::string>(modList["prefix"]) + "model_vars";
        if(_rxModels.exists(mvstr)){
          RObject obj1 = _rxModels.get(mvstr);
          if (rxIs(obj1, "rxModelVars")){
            return as<List>(obj1);
          }
        }
      }
    }
    // fileExists(const std::string& name)
    Environment RxODE("package:RxODE");
    Function f = as<Function>(RxODE["rxModelVars.character"]);
    return f(obj);
  } else if (rxIs(obj,"list")){
    bool params=false, lhs=false, state=false, trans=false, ini=false, model=false, md5=false, podo=false, dfdy=false;
    List lobj  = as<List>(obj);
    CharacterVector nobj = lobj.names();
    for (int i = 0; i < nobj.size(); i++){
      if (nobj[i] == "modVars"){
	return(rxModelVars_(lobj["modVars"]));
      } else if (!params && nobj[i]== "params"){
	params=true;
      } else if (!lhs && nobj[i] == "lhs"){
	lhs=true;
      } else if (!state && nobj[i] == "state"){
	state=true;
      } else if (!trans && nobj[i] == "trans"){
	trans=true;
      } else if (!ini && nobj[i] == "ini"){
	ini = true;
      } else if (!model && nobj[i] == "model"){
	model = true;
      } else if (!md5 && nobj[i] == "md5"){
	md5 = true;
      } else if (!podo && nobj[i] == "podo"){
	podo=true;
      } else if (!dfdy && nobj[i] == "dfdy"){
	dfdy = true;
      } else {
        return lobj;
      }
    }
    stop("Cannot figure out the model variables.");
  } else if (rxIs(obj,"NULL")) {
      stop("A NULL object does not have any RxODE model variables");
  } else {
    CharacterVector cls = obj.attr("class");
    int i = 0;
    Rprintf("Class:\t");
    for (i = 0; i < cls.size(); i++){
      Rprintf("%s\t", (as<std::string>(cls[i])).c_str());
    }
    Rprintf("\n");
    stop("Need an RxODE-type object to extract model variables from.");
  }
}


//' State variables
//'
//' This returns the model's compartments or states.
//'
//' @inheritParams rxModelVars
//'
//' @param state is a string indicating the state or compartment that
//'     you would like to lookup.
//'
//' @return If state is missing, return a character vector of all the states.
//'
//' If state is a string, return the compartment number of the named state.
//'
//' @seealso \code{\link{RxODE}}
//'
//' @author Matthew L.Fidler
//' @export
// [[Rcpp::export]]
RObject rxState(const RObject &obj = R_NilValue, RObject state = R_NilValue){
  List modVar = rxModelVars(obj);
  CharacterVector states = modVar["state"];
  if (state.isNULL()){
    return states;
  }
  else if (rxIs(state,"character")){
    CharacterVector lookup = as<CharacterVector>(state);
    if (lookup.size() > 1){
      // Fixme?
      stop("Can only lookup one state at a time.");
    }
    if (states.size() == 1){
      warning("Only one state variable should be input.");
    }
    IntegerVector ret(1);
    for (int i = 0; i < states.size(); i++){
      if (states[i] == lookup[0]){
	ret[0] = i+1;
	return ret;
      }
    }
    stop("Cannot locate compartment \"%s\".",as<std::string>(lookup[0]).c_str());
  }
  return R_NilValue;
}

//' Parameters specified by the model
//'
//' This return the model's parameters that are required to solve the
//' ODE system.
//'
//' @inheritParams rxModelVars
//'
//' @return a character vector listing the parameters in the model.
//'
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxParams(const RObject &obj){
  List modVar = rxModelVars(obj);
  CharacterVector ret = modVar["params"];
  return ret;
}


//' Jacobian and parameter derivatives
//'
//' Return Jacobain and parameter derivatives
//'
//' @inheritParams rxModelVars
//'
//' @return A list of the jacobian parameters defined in this RxODE
//'     object.
//' @author Matthew L. Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxDfdy(const RObject &obj){
  List modVar = rxModelVars(obj);
  CharacterVector ret = modVar["dfdy"];
  return ret;
}

//' Left handed Variables
//'
//' This returns the model calculated variables
//'
//' @inheritParams rxModelVars
//'
//' @return a character vector listing the calculated parameters
//' @seealso \code{\link{RxODE}}
//'
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxLhs(const RObject &obj){
  List modVar = rxModelVars(obj);
  CharacterVector ret = modVar["lhs"];
  return ret;
}
NumericVector rxInits0(const RObject &obj,
		       Nullable<NumericVector> vec = R_NilValue,
		       Nullable<CharacterVector> req = R_NilValue,
		       double defaultValue = 0,
		       bool noerror = false,
		       bool noini=false){
  NumericVector oini;
  CharacterVector cini;
  List modVar = rxModelVars(obj);
  if (!noini){
    oini = (modVar["ini"]);
    cini = oini.names();
  }
  int i, j, k;
  CharacterVector nreq;
  NumericVector miss;
  if (!req.isNull()){
    nreq = CharacterVector(req);
    if ((ISNA(defaultValue) && noerror) || !ISNA(defaultValue)){
      miss = NumericVector(nreq.size());
      for (i = 0; i < nreq.size(); i++) {
	miss[i] = defaultValue;
      }
      miss.attr("names") = CharacterVector(nreq);
    }
  }
  NumericVector nvec;
  CharacterVector nvecNames;
  if (!vec.isNull()){
    nvec = NumericVector(vec);
    if (nvec.size() > 0){
      if (!nvec.hasAttribute("names")){
	if (!req.isNull() && nreq.size() == nvec.size()){
	  nvec.attr("names") = req;
	  nvecNames = req;
	  std::string wstr = "Assumed order of inputs: ";
	  for (i = 0; i < nreq.size(); i++){
	    wstr += (i == 0 ? "" : ", ") + nreq[i];
	  }
	  warning(wstr);
	} else {
	  std::string sstr = "Length mismatch\nreq: c(";
	  for (i = 0; i < nreq.size(); i++){
	    sstr += (i == 0 ? "" : ", ") + nreq[i];
	  }
	  sstr += ")\nvec: c(";
	  for (i = 0; i < nvec.size(); i++){
            sstr += (i == 0 ? "" : ", ") + std::to_string((double)(nvec[i]));
          }
	  sstr += ")";
	  stop(sstr);
	}
      } else {
	nvecNames = nvec.names();
      }
    }
  }
  // Prefer c(vec, ini, miss)
  NumericVector ret;
  CharacterVector nret;
  if (!req.isNull()){
    ret =  NumericVector(nreq.size());
    bool found = false;
    for (i = 0; i < nreq.size(); i++){
      found = false;
      for (j = 0; !found && j < nvec.size(); j++){
	if (nreq[i] == nvecNames[j]){
	  found =  true;
	  ret[i] = nvec[j];
	  break;
	}
      }
      for (j = 0; !found && j < cini.size(); j++){
	if (nreq[i] == cini[j]){
          found =  true;
          ret[i] = oini[j];
          break;
        }
      }
      if (!found)
	ret[i] = miss[i];
    }
    ret.attr("names")= nreq;
  } else {
    // In this case
    // vec <- c(vec, ini);
    // vec <- vec[!duplicated(names(vec))]
    CharacterVector dupnames(nvec.size()+oini.size()+miss.size());
    j = 0;
    for (i = 0; i < nvec.size(); i++){
      dupnames[j] = nvecNames[i];
      j++;
    }
    for (i = 0; i < oini.size(); i++){
      dupnames[j] = cini[i];
      j++;
    }
    LogicalVector dups = duplicated(dupnames);
    j = 0;
    for (i = 0; i < dups.size(); i++){
      if (!dups[i]) j++;
    }
    ret = NumericVector(j);
    CharacterVector retn(j);
    k = 0, j = 0;
    for (i = 0; i < nvec.size(); i++){
      if (!dups[j]){
	ret[k] = nvec[i];
	retn[k] = nvecNames[i];
	k++;
      }
      j++;
    }
    for (i = 0; i < oini.size(); i++){
      if (!dups[j]){
	ret[k] = oini[i];
        retn[k] = cini[i];
        k++;
      }
      j++;
    }
    ret.attr("names") = retn;
  }
  return ret;
}
//' Initial Values and State values for a RxODE object
//'
//' Returns the initial values of the rxDll object
//'
//' @param obj rxDll, RxODE, or named vector representing default
//'     initial arguments
//'
//' @param vec If supplied, named vector for the model.
//'
//' @param req Required names, and the required order for the ODE solver
//'
//' @param defaultValue a number or NA representing the default value for
//'     parameters missing in \code{vec}, but required in \code{req}.
//'
//' @param noerror is a boolean specifying if an error should be thrown
//'     for missing parameter values when \code{default} = \code{NA}
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
NumericVector rxInits(const RObject &obj,
		      RObject vec = R_NilValue,
		      Nullable<CharacterVector> req = R_NilValue,
		      double defaultValue = 0,
		      bool noerror = false,
		      bool noini=false){
  if (vec.isNULL()){
    return rxInits0(obj, R_NilValue, req, defaultValue, noerror,noini);
  } else if (rxIs(vec, "list")){
    List vecL = as<List>(vec);
    Function unlist("unlist", R_BaseNamespace);
    NumericVector vec2 = as<NumericVector>(unlist(vec));
    // if (!vec2.hasAttribute("names")){
    //   stop("When using a list for inits or scales, the list must be named. list(depot=1)");
    // }
    if (vec2.size() != vecL.size()){
      stop("Only one estimate per named list item; i.e. list(x=1) instead of list(x=1:2).");
    }
    return rxInits0(obj, vec2, req, defaultValue, noerror,noini);
  } else if (rxIs(vec, "integer") || rxIs(vec, "numeric")){
    return rxInits0(obj, as<NumericVector>(vec), req, defaultValue, noerror,noini);
  } else {
    stop("Incompatible initial estimate type.");
  }
}

//' Setup the initial conditions.
//'
//' @param obj RxODE object
//' @param inits A numeric vector of initial conditions.
//' @author Matthew L. Fidler
//' @keywords internal
//' @export
//[[Rcpp::export]]
NumericVector rxSetupIni(const RObject &obj,
			   RObject inits = R_NilValue){
  List modVars = rxModelVars(obj);
  CharacterVector state = modVars["state"];
  return rxInits(obj, inits, state, 0.0);
}

//' Setup the initial conditions.
//'
//' @param obj RxODE object
//' @param inits A numeric vector of initial conditions.
//' @param extraArgs A list of extra args to parse for initial conditions.
//' @author Matthew L. Fidler
//' @keywords internal
//' @export
//[[Rcpp::export]]
NumericVector rxSetupScale(const RObject &obj,
			   RObject scale = R_NilValue,
			   Nullable<List> extraArgs = R_NilValue){
  List modVars = rxModelVars(obj);
  CharacterVector state = modVars["state"];
  NumericVector ret = rxInits(obj, scale, state, 1.0, false, true);
  unsigned int usedS = 0, foundS = 0;
  if (!extraArgs.isNull()){
    List xtra = as<List>(extraArgs);
    int i, n=state.size();
    std::string cur;
    for (i = 0; i < n; i++){
      cur = "S" + std::to_string(i+1);
      if (xtra.containsElementNamed(cur.c_str())){
	if (ret[i] == 1.0){
	  ret[i] = as<double>(xtra[cur]);
	  usedS++;
	} else {
	  stop("Trying to scale the same compartment by scale=c(%s=%f,...) and S%d=%f;  Cannot do both.",
	       (as<std::string>(state[i])).c_str(), ret[i], i+1,as<double>(xtra[i]));
	}
      } else {
	cur = "s" + std::to_string(i+1);
        if (xtra.containsElementNamed(cur.c_str())){
          if (ret[i] == 1.0){
            ret[i] = as<double>(xtra[cur]);
	    usedS++;
          } else {
            stop("Trying to scale the same compartment by scale=c(%s=%f,...) and s%d=%f;  Cannot do both.",
                 (as<std::string>(state[i])).c_str(), ret[i], i+1,as<double>(xtra[i]));
          }
        }
      }
    }
    Nullable<StringVector> xtraN2 = xtra.names();
    if (!xtraN2.isNull()){
      StringVector xtraN = StringVector(xtraN2);
      n = xtraN.size();
      std::string tmpstr;
      for (i = 0; i < n; i++){
	tmpstr = (as<std::string>(xtraN[i])).substr(0,1);
	if (tmpstr == "S" || tmpstr == "s"){
	  tmpstr = (as<std::string>(xtraN[i])).substr(1,1);
	  if (tmpstr == "0" || tmpstr == "1" || tmpstr == "2" || tmpstr == "3" || tmpstr == "4" ||
	      tmpstr == "5" || tmpstr == "6" || tmpstr == "7" || tmpstr == "8" || tmpstr == "9"){
	    foundS++;
          }
        }
      }
    }
    if (foundS > usedS){
      warning("Scaled a compartment that is not defined by the RxODE model.");
    }
  }
  return ret;
}

RObject rxSetupParamsThetaEta(const RObject &params = R_NilValue,
				    const RObject &theta = R_NilValue,
				    const RObject &eta = R_NilValue){
  // Now get the parameters as a data.frame
  if (rxIs(params,"list")) {
    Function asDf("as.data.frame", R_BaseNamespace);
    return rxSetupParamsThetaEta(asDf(params), theta, eta);
  }
  NumericMatrix parMat;
  int i;
  if (params.isNULL()){
    if (!theta.isNULL() || !eta.isNULL()){
      // Create the matrix
      NumericVector thetaN;
      if (rxIs(theta,"numeric") || rxIs(theta,"integer")){
        thetaN = as<NumericVector>(theta);
      } else if (rxIs(theta, "matrix")){
        NumericMatrix thetaM = as<NumericMatrix>(theta);
        if (thetaM.nrow() == 1){
          thetaN = NumericVector(thetaM.ncol());
          for (i = thetaM.ncol() ; i--;){
            thetaN[i] = thetaM(1,i);
          }
        } else if (thetaM.ncol() == 1){
          thetaN = NumericVector(thetaM.nrow());
          for (i = thetaM.ncol() ; i-- ;){
            thetaN[i] = thetaM(i, i);
          }
        } else {
          stop("'theta' is not compatible with params, check dimensions to make sure they are compatible.");
        }
      } else if (!theta.isNULL()){
        stop("'theta' is not compatible with params, check dimensions to make sure they are compatible.");
      }
      // Now eta
      NumericVector etaN;
      if (rxIs(eta,"numeric") || rxIs(eta,"integer")){
        etaN = as<NumericVector>(eta);
      } else if (rxIs(eta, "matrix")){
        NumericMatrix etaM = as<NumericMatrix>(eta);
        if (etaM.nrow() == 1){
          etaN = NumericVector(etaM.ncol());
          for (i = etaM.ncol() ; i-- ;){
            etaN[i] = etaM(0, i);
          }
        } else if (etaM.ncol() == 1){
          etaN = NumericVector(etaM.nrow());
          for (i = etaM.ncol() ; i--;){
            etaN[i] = etaM(i, 0);
          }
        } else {
          stop("'eta' is not compatible with params, check dimensions to make sure they are compatible.");
        }
      } else if (!eta.isNULL()){
        stop("'eta' is not compatible with params, check dimensions to make sure they are compatible.");
      }
      NumericMatrix tmp1(1, thetaN.size()+etaN.size());
      CharacterVector tmpN = CharacterVector(tmp1.size());
      for (i = thetaN.size(); i--;){
        tmp1(0, i) = thetaN[i];
        tmpN[i] = "THETA[" + std::to_string(i + 1) + "]";
      }
      i = thetaN.size();
      for (; i < thetaN.size()+ etaN.size(); i++){
        tmp1(0, i) = etaN[i - thetaN.size()];
        tmpN[i] = "ETA[" + std::to_string(i - thetaN.size() + 1) + "]";
      }
      tmp1.attr("dimnames") = List::create(R_NilValue, tmpN);
      parMat = tmp1;
    }
  } else if (rxIs(params, "data.frame") || rxIs(params, "matrix")){
    if (rxIs(params,"data.frame")){
      DataFrame tmp = as<DataFrame>(params);
      int nr = tmp.nrows();
      NumericMatrix tmpM(nr,tmp.size());
      for (i = tmp.size(); i-- ;){
        tmpM(_,i) = NumericVector(tmp[i]);
      }
      tmpM.attr("dimnames") = List::create(R_NilValue,tmp.names());
      parMat=tmpM;
    } else {
      parMat = as<NumericMatrix>(params);
    }
  } else if (rxIs(params, "numeric") || rxIs(params, "integer")){
    // Create the matrix
    NumericVector thetaN;
    if (rxIs(theta,"numeric") || rxIs(theta,"integer")){
      thetaN = as<NumericVector>(theta);
    } else if (rxIs(theta, "matrix")){
      NumericMatrix thetaM = as<NumericMatrix>(theta);
      if (thetaM.nrow() == 1){
        thetaN = NumericVector(thetaM.ncol());
        for (i = thetaM.ncol() ; i-- ;){
          thetaN[i] = thetaM(1,i);
        }
      } else if (thetaM.ncol() == 1){
        thetaN = NumericVector(thetaM.nrow());
        for (i = thetaM.ncol(); i--;){
          thetaN[i] = thetaM(i, i);
        }
      } else {
        stop("'theta' is not compatible with params, check dimensions to make sure they are compatible.");
      }
    } else if (!theta.isNULL()){
      stop("'theta' is not compatible with params, check dimensions to make sure they are compatible.");
    }
    // Now eta
    NumericVector etaN;
    if (rxIs(eta,"numeric") || rxIs(eta,"integer")){
      etaN = as<NumericVector>(eta);
    } else if (rxIs(eta, "matrix")){
      NumericMatrix etaM = as<NumericMatrix>(eta);
      if (etaM.nrow() == 1){
        etaN = NumericVector(etaM.ncol());
        for (i = etaM.ncol() ; i-- ;){
          etaN[i] = etaM(0, i);
        }
      } else if (etaM.ncol() == 1){
        etaN = NumericVector(etaM.nrow());
        for (i = etaM.ncol() ; i-- ;){
          etaN[i] = etaM(i, 0);
        }
      } else {
        stop("'eta' is not compatible with params, check dimensions to make sure they are compatible.");
      }
    } else if (!eta.isNULL()){
      stop("'eta' is not compatible with params, check dimensions to make sure they are compatible.");
    }
    NumericVector tmp0 = as<NumericVector>(params);
    NumericMatrix tmp1(1, tmp0.size()+thetaN.size()+etaN.size());
    CharacterVector tmp0N ;
    NumericVector pars = as<NumericVector>(params);
    if (tmp0.hasAttribute("names")){
      tmp0N = tmp0.names();
    } else if (tmp0.size() == pars.size()){
      tmp0N = pars;
    } else if (tmp0.size() > 0){
      // In this case there are no names
      stop("The parameter names must be specified.");
    }
    CharacterVector tmpN = CharacterVector(tmp1.size());
    for (i = 0; i < tmp0.size(); i++){
      tmp1(0, i) = tmp0[i];
      tmpN[i]   = tmp0N[i];
    }
    for (; i < tmp0.size()+thetaN.size(); i++){
      tmp1(0, i) = thetaN[i - tmp0.size()];
      tmpN[i] = "THETA[" + std::to_string(i - tmp0.size() + 1) + "]";
    }
    for (; i < tmp0.size()+thetaN.size()+ etaN.size(); i++){
      tmp1(0, i) = etaN[i - tmp0.size() - thetaN.size()];
      tmpN[i] = "ETA[" + std::to_string(i - tmp0.size() - thetaN.size() + 1) + "]";
    }
    tmp1.attr("dimnames") = List::create(R_NilValue, tmpN);
    parMat = tmp1;
  }
  return as<RObject>(parMat);
}


double *gsolve = NULL;
int gsolven = 0;
void gsolveSetup(int n){
  if (gsolven == 0){
    gsolve=Calloc(n+1024, double);
    gsolven=n+1024;
  } else if (n > gsolven){
    gsolve = Realloc(gsolve, n+1024, double);
    gsolven=n+1024;
  }
}

double *gInfusionRate = NULL;
int gInfusionRaten = 0;
void gInfusionRateSetup(int n){
  if (gInfusionRaten == 0){
    gInfusionRate=Calloc(n+1024, double);
    gInfusionRaten=n+1024;
  } else if (n > gInfusionRaten){
    gInfusionRate = Realloc(gInfusionRate, n+1024, double);
    gInfusionRaten=n+1024;
  }
}

double *gall_times = NULL;
int gall_timesn = 0;
void gall_timesSetup(int n){
  if (gall_timesn == 0){
    gall_times=Calloc(n+1024, double);
    gall_timesn=n+1024;
  } else if (n > gall_timesn){
    gall_times = Realloc(gall_times, n+1024, double);
    gall_timesn=n+1024;
  }
}


double *gamt = NULL;
int gamtn = 0;
void gamtSetup(int n){
  if (gamtn == 0){
    gamt=Calloc(n+1024, double);
    gamtn=n+1024;
  } else if (n > gamtn){
    gamt = Realloc(gamt, n+1024, double);
    gamtn=n+1024;
  }
}

double *glhs = NULL;
int glhsn = 0;
void glhsSetup(int n){
  if (glhsn == 0){
    glhs=Calloc(n+1024, double);
    glhsn=n+1024;
  } else if (n > glhsn){
    glhs = Realloc(glhs, n+1024, double);
    glhsn=n+1024;
  }
}

double *gcov = NULL;
int gcovn = 0;
void gcovSetup(int n){
  if (gcovn == 0){
    gcov=Calloc(n+1024, double);
    gcovn=n+1024;
  } else if (n > gcovn){
    gcov = Realloc(gcov, n+1024, double);
    gcovn=n+1024;
  }
}


double **gcovp = NULL;
int gcovpn = 0;
void gcovpSetup(int n){
  if (gcovpn == 0){
    gcovp=(double **) R_chk_calloc(n+100, sizeof(double*));
    gcovpn=n+100;
  } else if (n > gcovpn){
    gcovp = (double **) R_chk_realloc(gcovp, (n+100) * sizeof(double*));
    gcovpn=n+100;
  }
}


double *ginits = NULL;
int ginitsn = 0;
void ginitsSetup(int n){
  if (ginitsn == 0){
    ginits=Calloc(n+1024, double);
    ginitsn=n+1024;
  } else if (n > ginitsn){
    ginits = Realloc(ginits, n+1024, double);
    ginitsn=n+1024;
  }
}


double *gscale = NULL;
int gscalen = 0;
void gscaleSetup(int n){
  if (gscalen == 0){
    gscale=Calloc(n+1024, double);
    gscalen=n+1024;
  } else if (n > gscalen){
    gscale = Realloc(gscale, n+1024, double);
    gscalen=n+1024;
  }
}

double *gatol2 = NULL;
int gatol2n = 0;
void gatol2Setup(int n){
  if (gatol2n == 0){
    gatol2=Calloc(n+1024, double);
    gatol2n=n+1024;
  } else if (n > gatol2n){
    gatol2 = Realloc(gatol2, n+1024, double);
    gatol2n=n+1024;
  }
}

double *grtol2 = NULL;
int grtol2n = 0;
void grtol2Setup(int n){
  if (grtol2n == 0){
    grtol2=Calloc(n+1024, double);
    grtol2n=n+1024;
  } else if (n > grtol2n){
    grtol2 = Realloc(grtol2, n+1024, double);
    grtol2n=n+1024;
  }
}

double *gpars = NULL;
int gparsn = 0;
void gparsSetup(int n){
  if (gparsn == 0){
    gpars=Calloc(n, double);
    gparsn=n;
  } else if (n > gparsn){
    gpars = Realloc(gpars, n+1024, double);
    gparsn=n+1024;
  }
}


int *gevid = NULL;
int gevidn = 0;
void gevidSetup(int n){
  if (gevidn == 0){
    gevid=Calloc(n+1024, int);
    gevidn=n+1024;
  } else if (n > gevidn){
    gevid = Realloc(gevid, n+1024, int);
    gevidn=n+1024;
  }
}

int *gBadDose = NULL;
int gBadDosen = 0;
void gBadDoseSetup(int n){
  if (gBadDosen == 0){
    gBadDose=Calloc(n+1024, int);
    gBadDosen=n+1024;
  } else if (n > gBadDosen){
    gBadDose = Realloc(gBadDose, n+1024, int);
    gBadDosen=n+1024;
  }
}

int *grc = NULL;
int grcn = 0;
void grcSetup(int n){
  if (grcn == 0){
    grc=Calloc(n+1024, int);
    grcn=n+1024;
  } else if (n > grcn){
    grc = Realloc(grc, n+1024, int);
    grcn=n+1024;
  }
}

int *gidose = NULL;
int gidosen = 0;
extern "C" int *gidoseSetup(int n){
  if (gidosen == 0){
    gidose=Calloc(n+1024, int);
    gidosen=n+1024;
  } else if (n > gidosen){
    gidose = Realloc(gidose, n+1024, int);
    gidosen=n+1024;
  }
  return gidose;
}

int *gpar_cov = NULL;
int gpar_covn = 0;
void gpar_covSetup(int n){
  if (gpar_covn == 0){
    gpar_cov=Calloc(n+1024, int);
    gpar_covn=n+1024;
  } else if (n > gpar_covn){
    gpar_cov = Realloc(gpar_cov+1024, n, int);
    gpar_covn=n+1024;
  }
}

int *gParPos = NULL;
int gParPosn = 0;
void gParPosSetup(int n){
  if (gParPosn == 0){
    gParPos=Calloc(n+1024, int);
    gParPosn=n+1024;
  } else if (n > gParPosn){
    gParPos = Realloc(gParPos, n+1024, int);
    gParPosn=n+1024;
  }
}

int *gsvar = NULL;
int gsvarn = 0;
void gsvarSetup(int n){
  if (gsvarn == 0){
    gsvar=Calloc(n+1024, int);
    gsvarn=n+1024;
  } else if (n > gsvarn){
    gsvar = Realloc(gsvar, n+1024, int);
    gsvarn=n+1024;
  }
}

int *gsiV = NULL;
int gsiVn = 0;
extern "C" int *gsiVSetup(int n){
  if (gsiVn == 0){
    gsiV=Calloc(n, int);
    gsiVn=n;
  } else if (n > gsiVn){
    gsiV = Realloc(gsiV, n+1024, int);
    gsiVn=n+1024;
  }
  return gsiV;
}

extern "C" void rxOptionsIniData(){
  gsiVSetup(1024);
  gsvarSetup(1024);
  gpar_covSetup(1024);
  gidoseSetup(1024);
  grcSetup(1024);
  gBadDoseSetup(1024);
  gevidSetup(1024);
  gparsSetup(1024);
  grtol2Setup(1024);
  gatol2Setup(1024);
  gscaleSetup(1024);
  ginitsSetup(1024);
  gcovSetup(1024);
  glhsSetup(1024);
  gamtSetup(1024);
  gall_timesSetup(1024);
  gInfusionRateSetup(1024);
  gsolveSetup(1024);
  gParPosSetup(1024);
}



extern "C" void gFree(){
  if (gsiV != NULL) Free(gsiV);
  gsiVn=0;
  if (gsvar != NULL) Free(gsvar);
  gsvarn=0;
  if (gpar_cov != NULL) Free(gpar_cov);
  gpar_covn=0;
  if (gidose != NULL) Free(gidose);
  gidosen=0;
  if (grc != NULL) Free(grc);
  grcn=0;
  if (gBadDose != NULL) Free(gBadDose);
  gBadDosen=0;
  if (gevid != NULL) Free(gevid);
  gevidn=0;
  if (gpars != NULL) Free(gpars);
  gparsn=0;
  if (grtol2 != NULL) Free(grtol2);
  grtol2n=0;
  if (gatol2 != NULL) Free(gatol2);
  gatol2n=0;
  if (gscale != NULL) Free(gscale);
  gscalen=0;
  if (ginits != NULL) Free(ginits);
  ginitsn=0;
  if (gcov != NULL) Free(gcov);
  gcovn=0;
  if (glhs != NULL) Free(glhs);
  glhsn=0;
  if (gamt != NULL) Free(gamt);
  gamtn=0;
  if (gall_times != NULL) Free(gall_times);
  gall_timesn=0;
  if (gInfusionRate != NULL) Free(gInfusionRate);
  gInfusionRaten=0;
  if (gsolve != NULL) Free(gsolve);
  gsolven=0;
  if (gParPos != NULL) Free(gParPos);
  gParPosn = 0;
}

arma::mat rwish5(double nu, int p){
  GetRNGstate();
  arma::mat Z(p,p, fill::zeros);
  double curp = nu;
  double tmp =sqrt(Rf_rchisq(curp--));
  Z(0,0) = (tmp < 1e-100) ? 1e-100 : tmp;
  int i, j;
  if (p > 1){
    for (i = 1; i < (int)p; i++){
      tmp = sqrt(Rf_rchisq(curp--));
      Z(i,i) = (tmp < 1e-100) ? 1e-100 : tmp;
      for (j = 0; j < i; j++){
        // row,col
        Z(j,i) = norm_rand();
      }
    }
  }
  PutRNGstate();
  return Z;
}

NumericMatrix cvPost0(double nu, NumericMatrix omega, bool omegaIsChol = false,
                      bool returnChol = false){
  arma::mat S =as<arma::mat>(omega);
  int p = S.n_rows;
  if (p == 1){
    GetRNGstate();
    NumericMatrix ret(1,1);
    if (omegaIsChol){
      ret[0] = nu*omega[0]*omega[0]/(Rf_rgamma(nu/2.0,2.0));
    } else {
      ret[0] = nu*omega[0]/(Rf_rgamma(nu/2.0,2.0));
    }
    if (returnChol) ret[0] = sqrt(ret[0]);
    PutRNGstate();
    return ret;
  } else {
    arma::mat Z = rwish5(nu, p);
    // Backsolve isn't available in armadillo
    arma::mat Z2 = arma::trans(arma::inv(trimatu(Z)));
    arma::mat cv5;
    if (omegaIsChol){
      cv5 = S;
    } else {
      cv5 = arma::chol(S);
    }
    arma::mat mat1 = Z2 * cv5;
    mat1 = mat1.t() * mat1;
    mat1 = mat1 * nu;
    if (returnChol) mat1 = arma::chol(mat1);
    return wrap(mat1);
  }
}

//' Sample a covariance Matrix from the Posteior Inverse Wishart distribution.
//'
//' Note this Inverse wishart rescaled to match the original scale of the covariance matrix.
//'
//' If your covariance matrix is a 1x1 matrix, this uses an scaled inverse chi-squared which 
//' is equivalent to the Inverse Wishart distribution in the uni-directional case.
//'
//' @param nu Degrees of Freedom (Number of Observations) for 
//'        covariance matrix simulation.
//' @param omega Estimate of Covariance matrix.
//' @param n Number of Matricies to sample.  By default this is 1.
//' @param omegaIsChol is an indicator of if the omega matrix is in the cholesky decomposition. 
//' @param returnChol Return the cholesky decomposition of the covariance matrix sample.
//'
//' @return a matrix (n=1) or a list of matricies (n > 1)
//'
//' @author Matthew L.Fidler & Wenping Wang
//' 
//' @export
//[[Rcpp::export]]
RObject cvPost(double nu, RObject omega, int n = 1, bool omegaIsChol = false, bool returnChol = false){
  if (n == 1){
    if (rxIs(omega,"numeric.matrix") || rxIs(omega,"integer.matrix")){
      return as<RObject>(cvPost0(nu, as<NumericMatrix>(omega), omegaIsChol));
    } else if (rxIs(omega, "numeric") || rxIs(omega, "integer")){
      NumericVector om1 = as<NumericVector>(omega);
      if (om1.size() % 2 == 0){
        int n1 = om1.size()/2;
        NumericMatrix om2(n1,n1);
        for (int i = 0; i < om1.size();i++){
          om2[i] = om1[i];
        }
        return as<RObject>(cvPost0(nu, om2, omegaIsChol, returnChol));
      }
    }
  } else {
    List ret(n);
    for (int i = 0; i < n; i++){
      ret[i] = cvPost(nu, omega, 1, omegaIsChol, returnChol);
    }
    return(as<RObject>(ret));
  }
  stop("omega needs to be a matrix or a numberic vector that can be converted to a matrix.");
  return R_NilValue;
}

//' Scaled Inverse Chi Squared distribution
//'
//' @param n Number of random samples
//' @param nu degrees of freedom of inverse chi square
//' @param scale  Scale of inverse chi squared distribution 
//'         (default is 1).
//' @return a vector of inverse chi squared deviates .
//' @export
//[[Rcpp::export]]
NumericVector rinvchisq(const int n = 1, const double &nu = 1.0, const double &scale = 1){
  NumericVector ret(n);
  GetRNGstate();
  for (int i = 0; i < n; i++){
    ret[i] = nu*scale/(Rf_rgamma(nu/2.0,2.0));
  }
  PutRNGstate();
  return ret;
}

extern "C" double *rxGetErrs(){
  getRxModels();
  if (_rxModels.exists(".sigma")){
    NumericMatrix sigma = _rxModels[".sigma"];
    return &sigma[0];
  }
  return NULL;
}

extern "C" int rxGetErrsNcol(){
  getRxModels();
  if (_rxModels.exists(".sigma")){
    NumericMatrix sigma = _rxModels[".sigma"];
    int ret = sigma.ncol();
    return ret;
  } 
  return 0;
}
  
SEXP rxGetFromChar(char *ptr, std::string var){
  std::string str(ptr);
  // Rcout << str << "\n";
  CharacterVector cv(1);
  cv[0] = str;
  List mv = rxModelVars(as<RObject>(cv));
  if (var == ""){
    return wrap(mv);
  } else {
    return wrap(mv[var]);
  }
}

//' Simulate Parameters from a Theta/Omega specification
//'
//' @param params Named Vector of RxODE model parameters
//'
//' @param thetaMat Named theta matrix.
//'
//' @param thetaDf The degrees of freedom of a t-distribution for
//'     simulation.  By default this is \code{NULL} which is
//'     equivalent to \code{Inf} degrees, or to simulate from a normal
//'     distribution instead of a t-distribution.
//'
//' @param thetaIsChol Indicates if the \code{theta} supplied is a
//'     Cholesky decomposed matrix instead of the traditional
//'     symmetric matrix.
//'
//' @param nSub Number between subject variabilities (ETAs) simulated for every 
//'        realization of the parameters.
//'
//' @param omega Named omega matrix.
//'
//' @param omegaDf The degrees of freedom of a t-distribution for
//'     simulation.  By default this is \code{NULL} which is
//'     equivalent to \code{Inf} degrees, or to simulate from a normal
//'     distribution instead of a t-distribution.
//'
//' @param omegaIsChol Indicates if the \code{omega} supplied is a
//'     Cholesky decomposed matrix instead of the traditional
//'     symmetric matrix.
//'
//' @param nStud Number virtual studies to characterize uncertainty in estimated 
//'        parameters.
//'
//' @param sigma Matrix for residual variation.  Adds a "NA" value for each of the 
//'     indivdual parameters, residuals are updated after solve is completed. 
//'
//' @inheritParams rxSolve
//'
//' @param simVariability For each study simulate the uncertanty in the Omega and 
//'       Sigma item
//'
//' @param nObs Number of observations to simulate for sigma.
//'
//' @author Matthew L.Fidler
//'
//' @export
//[[Rcpp::export]]
List rxSimThetaOmega(const Nullable<NumericVector> &params    = R_NilValue,
                     const Nullable<NumericMatrix> &omega= R_NilValue,
                     const Nullable<NumericVector> &omegaDf= R_NilValue,
                     const bool &omegaIsChol = false,
                     unsigned int nSub = 1,
                     const Nullable<NumericMatrix> &thetaMat = R_NilValue,
                     const Nullable<NumericVector> &thetaDf  = R_NilValue,
                     const bool &thetaIsChol = false,
                     unsigned int nStud = 1,
                     const Nullable<NumericMatrix> sigma = R_NilValue,
                     const Nullable<NumericVector> &sigmaDf= R_NilValue,
                     const bool &sigmaIsChol = false,
                     unsigned int nCoresRV = 1,
                     unsigned int nObs = 1,
                     bool simVariability = true){
  NumericVector par;
  if (params.isNull()){
    stop("This function requires overall parameters.");
  } else {
    par = NumericVector(params);
    if (!par.hasAttribute("names")){
      stop("'params' must be a named vector.");
    }
  }
  bool simSigma = false;
  NumericMatrix sigmaM;
  CharacterVector sigmaN;
  NumericMatrix sigmaMC;
  if (!sigma.isNull() && nObs > 1){
    simSigma = true;
    sigmaM = as<NumericMatrix>(sigma);
    if (!sigmaM.hasAttribute("dimnames")){
      stop("'sigma' must be a named Matrix.");
    }
    if (sigmaIsChol){
      sigmaMC = sigmaM;
    } else {
      sigmaMC = wrap(arma::chol(as<arma::mat>(sigmaM)));
    }
    sigmaN = as<CharacterVector>((as<List>(sigmaM.attr("dimnames")))[1]);
  }  
  unsigned int scol = 0;
  if (simSigma){
    scol = sigmaMC.ncol();
    if (nObs*nStud*nSub*scol < 0){
      // nStud = INT_MAX/(nObs*nSub*scol)*0.25;
      stop("Simulation Overflow; Reduce the number of observations, number of subjects or number of studies.");
    }
  }
  NumericMatrix thetaM;
  CharacterVector thetaN;
  bool simTheta = false;
  CharacterVector parN = CharacterVector(par.attr("names"));
  IntegerVector thetaPar(parN.size());
  unsigned int i, j, k;
  if (!thetaMat.isNull() && nStud > 1){
    thetaM = as<NumericMatrix>(thetaMat);
    if (!thetaM.hasAttribute("dimnames")){
      stop("'thetaMat' must be a named Matrix.");
    }
    thetaM = as<NumericMatrix>(rxSimSigma(as<RObject>(thetaMat), as<RObject>(thetaDf), nCoresRV, thetaIsChol, nStud));
    thetaN = as<CharacterVector>((as<List>(thetaM.attr("dimnames")))[1]);
    for (i = 0; i < parN.size(); i++){
      thetaPar[i] = -1;
      for (j = 0; j < thetaN.size(); j++){
        if (parN[i] == thetaN[j]){
          thetaPar[i] = j;
          break;
        }
      }
    }
    simTheta = true;
  } else if (!thetaMat.isNull() && nStud <= 1){
    warning("'thetaMat' is ignored since nStud <= 1.");
  }
  bool simOmega = false;
  NumericMatrix omegaM;
  CharacterVector omegaN;
  NumericMatrix omegaMC;
  if (!omega.isNull() && nSub > 1){
    simOmega = true;
    omegaM = as<NumericMatrix>(omega);
    if (!omegaM.hasAttribute("dimnames")){
      stop("'omega' must be a named Matrix.");
    }
    if (omegaIsChol){
      omegaMC = omegaM;
    } else {
      omegaMC = wrap(arma::chol(as<arma::mat>(omegaM)));
    }
    omegaN = as<CharacterVector>((as<List>(omegaM.attr("dimnames")))[1]);
  } else if (nSub > 1){
    stop("'omega' is required for multi-subject simulations.");
  }
  // Now create data frame of parameter values
  List omegaList;
  List sigmaList;  
  if (simVariability && nStud > 1){
    if (simOmega) {
      omegaList = cvPost((double)nSub, as<RObject>(omegaMC), nStud,  true, true);
    }
    if (simSigma){
      sigmaList = cvPost((double)nObs, as<RObject>(sigmaMC), nStud,  true, true);
    }
  }
  unsigned int pcol = par.size();
  unsigned int ocol = 0;
  unsigned int ncol = pcol;
  if (simOmega){
    ocol = omegaMC.ncol();
    ncol += ocol;
  }
  NumericMatrix ret1;
  if (simSigma){
    ncol += scol;
    ret1 = NumericMatrix(nObs*nStud*nSub, scol);
  }
  List ret0(ncol);
  NumericVector nm;
  NumericMatrix nm1;
  for (i = 0; i < ncol; i++){
    nm = NumericVector(nSub*nStud);
    ret0[i] = nm;
  }
  for (i = 0; i < nStud; i++){
    for (j = 0; j < pcol; j++){
      nm = ret0[j];
      for (k = 0; k < nSub; k++){
        nm[nSub*i + k] = par[j];
      }
      if (simTheta){
        if(thetaPar[j] != -1){
          for (k = 0; k < nSub; k++){
            nm[nSub*i + k] += thetaM(i, thetaPar[j]);
          }
        }
      }
      ret0[j] = nm;
    }
    // Now Omega Covariates
    if (ocol > 0){
      if (simVariability && nStud > 1){
        // nm = ret0[j]; // parameter column
        nm1 = as<NumericMatrix>(rxSimSigma(as<RObject>(omegaList[i]), as<RObject>(omegaDf), nCoresRV, true, nSub,false));
      } else {
        nm1 = as<NumericMatrix>(rxSimSigma(as<RObject>(omegaMC), as<RObject>(omegaDf), nCoresRV, true, nSub,false));
      }
      for (j=pcol; j < pcol+ocol; j++){
        nm = ret0[j];
        for (k = 0; k < nSub; k++){
          nm[nSub*i + k] = nm1(k, j-pcol);
        }
        ret0[j] = nm;
      }
    }
    if (scol > 0){
      if (simVariability  && nStud > 1){
        nm1 = as<NumericMatrix>(rxSimSigma(as<RObject>(sigmaList[i]), as<RObject>(sigmaDf), nCoresRV, true, nObs*nSub, false));
      } else {
        nm1 = as<NumericMatrix>(rxSimSigma(as<RObject>(sigmaMC), as<RObject>(sigmaDf), nCoresRV, true, nObs*nSub, false));
      }
      for (j = 0; j < scol; j++){
        for (k = 0; k < nObs*nSub; k++){
          // ret1 = NumericMatrix(nObs*nStud, scol);
          ret1(nObs*nSub*i+k, j) = nm1(k, j);
        }
      }
    }
  }
  CharacterVector dfName(ncol);
  for (i = 0; i < pcol; i++){
    dfName[i] = parN[i];
  }
  for (i = pcol; i < pcol+ocol; i++){
    dfName[i] = omegaN[i-pcol];
  }
  for (i = pcol+ocol; i < ncol; i++){
    dfName[i] = sigmaN[i-pcol-ocol];
  }
  ret0.attr("names") = dfName;
  ret0.attr("class") = "data.frame";
  ret0.attr("row.names") = IntegerVector::create(NA_INTEGER,-nSub*nStud);
  ret1.attr("dimnames") = List::create(R_NilValue, sigmaN);
  getRxModels();
  _rxModels[".sigma"] = ret1;
  if (simTheta){
    _rxModels[".theta"] = thetaM;
  }
  if (simVariability && nStud > 1){
    _rxModels[".omegaL"] = omegaList;
    _rxModels[".sigmaL"] =sigmaList;
  }
  return ret0;
}


extern "C" double *global_InfusionRate(unsigned int mx);

#define defrx_params R_NilValue
#define defrx_events R_NilValue
#define defrx_inits R_NilValue
#define defrx_covs R_NilValue
#define defrx_method 2
#define defrx_transit_abs R_NilValue
#define defrx_atol 1.0e-8
#define defrx_rtol 1.0e-8
#define defrx_maxsteps 5000
#define defrx_hmin 0
#define defrx_hmax R_NilValue
#define defrx_hini 0
#define defrx_maxordn 12
#define defrx_maxords 5
#define defrx_cores 1
#define defrx_covs_interpolation 0
#define defrx_addCov false
#define defrx_matrix false
#define defrx_sigma  R_NilValue
#define defrx_sigmaDf R_NilValue
#define defrx_nCoresRV 1
#define defrx_sigmaIsChol false
#define defrx_nDisplayProgress 10000
#define defrx_amountUnits NA_STRING
#define defrx_timeUnits "hours"
#define defrx_addDosing false


RObject rxCurObj;

Nullable<Environment> rxRxODEenv(RObject obj);

std::string rxDll(RObject obj);

bool rxDynLoad(RObject obj);

extern "C" rx_solving_options_ind *rxOptionsIniEnsure(int mx);
extern "C" void RxODE_assign_fn_pointers(SEXP);
SEXP rxSolveC(const RObject &object,
	      const Nullable<CharacterVector> &specParams = R_NilValue,
	      const Nullable<List> &extraArgs = R_NilValue,
	      const RObject &params = R_NilValue,
	      const RObject &events = R_NilValue,
	      const RObject &inits = R_NilValue,
	      const RObject &scale = R_NilValue,
	      const RObject &covs  = R_NilValue,
	      const int method = 2,
	      const Nullable<LogicalVector> &transit_abs = R_NilValue,
	      const double atol = 1.0e-8,
	      const double rtol = 1.0e-6,
	      const int maxsteps = 5000,
	      const double hmin = 0,
	      const Nullable<NumericVector> &hmax = R_NilValue,
	      const double hini = 0,
	      const int maxordn = 12,
	      const int maxords = 5,
	      const int cores = 1,
	      const int covs_interpolation = 0,
	      bool addCov = false,
	      bool matrix = false,
	      const Nullable<NumericMatrix> &sigma= R_NilValue,
	      const Nullable<NumericVector> &sigmaDf= R_NilValue,
	      const unsigned int &nCoresRV= 1,
	      const bool &sigmaIsChol= false,
	      const int &nDisplayProgress = 10000,
	      const CharacterVector &amountUnits = NA_STRING,
	      const CharacterVector &timeUnits = "hours",
	      const bool addDosing = false,
	      const RObject &theta = R_NilValue,
	      const RObject &eta = R_NilValue,
	      const bool updateObject = false,
	      const bool doSolve = true,
              const Nullable<NumericMatrix> &omega = R_NilValue, 
	      const Nullable<NumericVector> &omegaDf = R_NilValue, 
	      const bool &omegaIsChol = false,
              const unsigned int nSub = 1, 
	      const Nullable<NumericMatrix> &thetaMat = R_NilValue, 
	      const Nullable<NumericVector> &thetaDf = R_NilValue, 
	      const bool &thetaIsChol = false,
              const unsigned int nStud = 1, 
	      const bool simVariability=true){
  bool isRxSolve = rxIs(object, "rxSolve");
  bool isEnvironment = rxIs(object, "environment");
  if (isRxSolve || isEnvironment){
    bool update_params = false,
      update_events = false,
      update_inits = false,
      update_covs = false,
      update_method = false,
      update_transit_abs = false,
      update_atol = false,
      update_rtol = false,
      update_maxsteps = false,
      update_hini = false,
      update_hmin = false,
      update_hmax = false,
      update_maxordn = false,
      update_maxords = false,
      update_cores = false,
      update_covs_interpolation = false,
      update_addCov = false,
      update_matrix = false,
      update_sigma  = false,
      update_sigmaDf = false,
      update_nCoresRV = false,
      update_sigmaIsChol = false,
      update_amountUnits = false,
      update_timeUnits = false,
      update_scale = false,
      update_dosing = false;
    if (specParams.isNull()){
      warning("No additional parameters were specified; Returning fit.");
      return object;
    }
    CharacterVector specs = CharacterVector(specParams);
    int n = specs.size(), i;
    for (i = n; i--;){
      if (as<std::string>(specs[i]) == "params")
	update_params = true;
      else if (as<std::string>(specs[i]) == "events")
	update_events = true;
      else if (as<std::string>(specs[i]) == "inits")
	update_inits = true;
      else if (as<std::string>(specs[i]) == "covs")
	update_covs = true;
      else if (as<std::string>(specs[i]) == "method")
	update_method = true;
      else if (as<std::string>(specs[i]) == "transit_abs")
	update_transit_abs = true;
      else if (as<std::string>(specs[i]) == "atol")
	update_atol = true;
      else if (as<std::string>(specs[i]) == "rtol")
	update_rtol = true;
      else if (as<std::string>(specs[i]) == "maxsteps")
	update_maxsteps = true;
      else if (as<std::string>(specs[i]) == "hmin")
	update_hmin = true;
      else if (as<std::string>(specs[i]) == "hmax")
	update_hmax = true;
      else if (as<std::string>(specs[i]) == "maxordn")
	update_maxordn = true;
      else if (as<std::string>(specs[i]) == "maxords")
	update_maxords = true;
      else if (as<std::string>(specs[i]) == "cores")
	update_cores = true;
      else if (as<std::string>(specs[i]) == "covs_interpolation")
	update_covs_interpolation = true;
      else if (as<std::string>(specs[i]) == "addCov")
	update_addCov = true;
      else if (as<std::string>(specs[i]) == "matrix")
	update_matrix = true;
      else if (as<std::string>(specs[i]) == "sigma")
	update_sigma  = true;
      else if (as<std::string>(specs[i]) == "sigmaDf")
	update_sigmaDf = true;
      else if (as<std::string>(specs[i]) == "nCoresRV")
	update_nCoresRV = true;
      else if (as<std::string>(specs[i]) == "sigmaIsChol")
	update_sigmaIsChol = true;
      else if (as<std::string>(specs[i]) == "amountUnits")
	update_amountUnits = true;
      else if (as<std::string>(specs[i]) == "timeUnits")
	update_timeUnits = true;
      else if (as<std::string>(specs[i]) == "hini")
	update_hini = true;
      else if (as<std::string>(specs[i]) == "scale")
	update_scale = true;
      else if (as<std::string>(specs[i]) == "addDosing")
	update_dosing = true;
    }
    // Now update
    Environment e;
    List obj;
    if (isRxSolve){
      obj = as<List>(obj);
      CharacterVector classattr = object.attr("class");
      e = as<Environment>(classattr.attr(".RxODE.env"));
    } else  { // if (rxIs(object, "environment")) 
      e = as<Environment>(object);
      obj = as<List>(e["obj"]);
    }
    getRxModels();
    if(e.exists(".sigma")){
      _rxModels[".sigma"]=as<NumericMatrix>(e[".sigma"]);
    }
    if(e.exists(".sigmaL")){
      _rxModels[".sigmaL"]=as<List>(e[".sigmaL"]);
    }
    if(e.exists(".omegaL")){
      _rxModels[".omegaL"] = as<List>(e[".omegaL"]);
    }
    if(e.exists(".theta")){
      _rxModels[".theta"] = as<NumericMatrix>(e[".theta"]);
    }
    RObject new_params = update_params ? params : e["args.params"];
    RObject new_events = update_events ? events : e["args.events"];
    RObject new_inits = update_inits ? inits : e["args.inits"];
    RObject new_covs  = update_covs  ? covs  : e["args.covs"];
    int new_method = update_method ? method : e["args.method"];
    Nullable<LogicalVector> new_transit_abs = update_transit_abs ? transit_abs : e["args.transit_abs"];
    double new_atol = update_atol ? atol : e["args.atol"];
    double new_rtol = update_rtol ? rtol : e["args.rtol"];
    int new_maxsteps = update_maxsteps ? maxsteps : e["args.maxsteps"];
    int new_hmin = update_hmin ? hmin : e["args.hmin"];
    Nullable<NumericVector> new_hmax = update_hmax ? hmax : e["args.hmax"];
    int new_hini = update_hini ? hini : e["args.hini"];
    int new_maxordn = update_maxordn ? maxordn : e["args.maxordn"];
    int new_maxords = update_maxords ? maxords : e["args.maxords"];
    int new_cores = update_cores ? cores : e["args.cores"];
    int new_covs_interpolation = update_covs_interpolation ? covs_interpolation : e["args.covs_interpolation"];
    bool new_addCov = update_addCov ? addCov : e["args.addCov"];
    bool new_matrix = update_matrix ? matrix : e["args.matrix"];
    Nullable<NumericMatrix> new_sigma = update_sigma ? sigma : e["args.sigma"];
    Nullable<NumericVector> new_sigmaDf = update_sigmaDf ? sigmaDf : e["args.sigmaDf"];
    unsigned int new_nCoresRV = update_nCoresRV ? nCoresRV : e["args.nCoresRV"];
    bool new_sigmaIsChol = update_sigmaIsChol ? sigmaIsChol : e["args.sigmaIsChol"];
    int new_nDisplayProgress = e["args.nDisplayProgress"];
    CharacterVector new_amountUnits = update_amountUnits ? amountUnits : e["args.amountUnits"];
    CharacterVector new_timeUnits = update_timeUnits ? timeUnits : e["args.timeUnits"];
    RObject new_scale = update_scale ? scale : e["args.scale"];
    bool new_addDosing = update_dosing ? addDosing : e["args.addDosing"];

    RObject new_object = as<RObject>(e["args.object"]);
    List dat = as<List>(rxSolveC(new_object, R_NilValue, extraArgs, new_params, new_events, new_inits, new_scale, new_covs,
				 new_method, new_transit_abs, new_atol, new_rtol, new_maxsteps, new_hmin,
				 new_hmax, new_hini,new_maxordn, new_maxords, new_cores, new_covs_interpolation,
				 new_addCov, new_matrix, new_sigma, new_sigmaDf, new_nCoresRV, new_sigmaIsChol,
				 new_nDisplayProgress, new_amountUnits, new_timeUnits, new_addDosing));
    if (updateObject && as<bool>(e[".real.update"])){
      List old = as<List>(rxCurObj);
      //Should I zero out the List...?
      CharacterVector oldNms = old.names();
      CharacterVector nms = dat.names();
      if (oldNms.size() == nms.size()){
	int i;
	for (i = 0; i < nms.size(); i++){
	  old[as<std::string>(nms[i])] = as<SEXP>(dat[as<std::string>(nms[i])]);
	}
	old.attr("class") = dat.attr("class");
	old.attr("row.names") = dat.attr("row.names");
	return old;
      } else {
	warning("Cannot update object...");
	return dat;
      }
    }
    e[".real.update"] = true;
    return dat;
  } else {
    // Load model
    if (!rxDynLoad(object)){
      stop("Cannot load RxODE dlls for this model.");
    }
    // Get model 
    List mv = rxModelVars(object);
    // Assign Pointers
    RxODE_assign_fn_pointers(as<SEXP>(mv));
    // Get the C solve object
    rx_solve* rx = getRxSolve_();
    rx_solving_options* op = rx->op;
    rx_solving_options_ind* ind;
    
    rx->matrix = (int)(matrix);
    rx->add_cov = (int)(addCov);
    op->stiff = method;
    if (method != 2){
      op->cores =1;
    } else {
      op->cores=cores;
    }
    // Now set up events and parameters
    RObject par0 = params;
    RObject ev0  = events;
    RObject ev1;
    RObject par1;
    bool swappedEvents = false;
    NumericVector initsC;
    if (rxIs(par0, "rx.event")){
      // Swapped events and parameters
      swappedEvents=true;
      ev1 = par0;
      par1 = ev0;
    } else if (rxIs(ev0, "rx.event")) {
      ev1 = ev0;
      par1 = par0;
    } else {
      Rcout << "Events:\n";
      Rcout << "Parameters:\n";
      stop("Need some event information (observation/dosing times) to solve.\nYou can use either 'eventTable' or an RxODE compatible data.frame/matrix.");
    }
    // Now get the parameters (and covariates)
    //
    // Unspecified parameters can be found in the modVars["ini"]
    NumericVector mvIni = mv["ini"];
    // The event table can contain covariate information, if it is acutally a data frame or matrix.
    Nullable<CharacterVector> covnames0, simnames0;
    CharacterVector covnames, simnames;
    CharacterVector pars = mv["params"];
    int npars = pars.size();
    CharacterVector state = mv["state"];
    CharacterVector lhs = mv["lhs"];
    int lhsSize = lhs.size();
    op->neq = state.size();
    op->badSolve = 0;
    op->abort = 0;
    op->ATOL = atol;          //absolute error
    op->RTOL = rtol;          //relative error
    gatol2Setup(op->neq);
    grtol2Setup(op->neq);
    std::fill_n(&gatol2[0],op->neq, atol);
    std::fill_n(&grtol2[0],op->neq, rtol);
    op->atol2 = &gatol2[0];
    op->rtol2 = &grtol2[0];
    op->H0 = hini;
    op->HMIN = hmin;
    op->mxstep = maxsteps;
    op->MXORDN = maxordn;
    op->MXORDS = maxords;
    // The initial conditions cannot be changed for each individual; If
    // they do they need to be a parameter.
    initsC = rxInits(object, inits, state, 0.0);
    ginitsSetup(initsC.size());
    std::copy(initsC.begin(), initsC.end(), &ginits[0]);
    op->inits = &ginits[0];
    NumericVector scaleC = rxSetupScale(object, scale, extraArgs);
    gscaleSetup(scaleC.size());
    std::copy(scaleC.begin(),scaleC.end(),&gscale[0]);
    op->scale = &gscale[0];
    //
    int transit = 0;
    if (transit_abs.isNull()){
      transit = mv["podo"];
      if (transit){
        warning("Assumed transit compartment model since 'podo' is in the model.");
      }
    }  else {
      LogicalVector tr = LogicalVector(transit_abs);
      if (tr[0]){
        transit=  1;
      }
    }
    op->do_transit_abs = transit;
    op->nlhs = lhs.size();
    CharacterVector trans = mv["trans"];
    // Make sure the model variables are assigned...
    // This fixes random issues on windows where the solves are done and the data set cannot be solved.
    std::string ptrS = (as<std::string>(trans["model_vars"]));
    getRxModels();
    _rxModels[ptrS] = mv;
    sprintf(op->modNamePtr, "%s", ptrS.c_str());
    // approx fun options
    op->is_locf = covs_interpolation;
    if (op->is_locf == 0){//linear
      op->f1 = 1.0;
      op->f2 = 0.0;
      op->kind=1;
    } else if (op->is_locf == 1){ // locf
      op->f2 = 0.0;
      op->f1 = 1.0;
      op->kind = 0;
      op->is_locf=1;
    } else if (op->is_locf == 2){ //nocb
      op->f2 = 1.0;
      op->f1 = 0.0;
      op->kind = 0;
    }  else if (op->is_locf == 3){ // midpoint
      op->f1 = op->f2 = 0.5;
      op->kind = 0;
    } else {
      stop("Unknown covariate interpolation specified.");
    }
    op->extraCmt=0;
    op->nDisplayProgress = nDisplayProgress;
    op->ncoresRV = nCoresRV;
    op->isChol = (int)(sigmaIsChol);
    unsigned int nsub = 0, nsim = 0;
    unsigned int nobs = 0, ndoses = 0;
    unsigned int i, j, k = 0;
    int ncov =0, curcovi = 0, curcovpi = 0;
    double tmp, hmax1, hmax2, tlast;
    // Covariate options
    // Simulation variabiles
    // int *svar;
    if (rxIs(ev1, "EventTable")){
      rxOptionsIniEnsure(1);
      ind = &(rx->subjects[0]);
      ind->slvr_counter   = 0;
      ind->dadt_counter   = 0;
      ind->jac_counter    = 0;
      ind->id=1;
      List et = List(ev1);
      Function f = et["get.EventTable"];
      DataFrame dataf = f();
      NumericVector time = as<NumericVector>(dataf[0]);
      IntegerVector evid = as<IntegerVector>(dataf[1]);
      NumericVector amt  = as<NumericVector>(dataf[2]);
      // Time copy
      ind->n_all_times   = time.size();
      gall_timesSetup(ind->n_all_times);
      std::copy(time.begin(), time.end(), &gall_times[0]);
      ind->all_times     = &gall_times[0];
      // EVID copy
      gevidSetup(ind->n_all_times);
      std::copy(evid.begin(),evid.end(), &gevid[0]);
      ind->evid     = &gevid[0];
      j=0;
      gamtSetup(ind->n_all_times);
      ind->dose = &gamt[0];
      // Slower
      tlast = time[0];
      hmax1 = hmax2 = 0;
      gidoseSetup(ind->n_all_times);
      for (i =0; i != (unsigned int)(ind->n_all_times); i++){
        if (ind->evid[i]){
          ndoses++;
	  gidose[j] = i;
          gamt[j++] = amt[i];
	} else {
	  nobs++;
	  tmp = time[i]-tlast;
	  if (tmp < 0) stop("Dataset must be ordered by ID and TIME variables.");
	  if (tmp > hmax1){
	    hmax1 = tmp;
	  }
	}
      }
      ind->idose = &gidose[0];
      ind->ndoses = ndoses;
      rx->nobs = nobs;
      rx->nall = nobs+ndoses;
      if (!hmax.isNull()){
	NumericVector hmax0 = NumericVector(hmax);
	ind->HMAX = hmax0[0];
	op->hmax2 = hmax0[0];
      } else {
        ind->HMAX = hmax1;
	op->hmax2 = hmax1;
      }
      nsub=1;
      nsim=1;
      if (!covs.isNULL()){
        // op->do_par_cov = 1;
	op->do_par_cov = 1;
	ncov = 0;
        if (rxIs(covs,"data.frame")){
	  List df = as<List>(covs);
          CharacterVector dfNames = df.names();
	  int dfN = dfNames.size();
	  gcovpSetup(dfN);
          gcovSetup(dfN * ind->n_all_times);
	  gpar_covSetup(dfN);
	  k = 0;
	  for (i = dfN; i--;){
	    for (j = npars; j--;){
	      if (pars[j] == dfNames[i]){
		gpar_cov[k] = j+1;
		// Not clear if this is an integer/real.  Copy the values.
		NumericVector cur = as<NumericVector>(df[i]);
		std::copy(cur.begin(), cur.end(), &(gcov[curcovi]));
                gcovp[curcovpi++] = &gcov[curcovi];
                curcovi += ind->n_all_times;
		ncov++;
		k++;
		break;
	      }
	    }
	  }
          op->ncov=ncov;
          ind->cov_ptr = &(gcovp[0]);
	  op->par_cov=&(gpar_cov[0]);
        } else if (rxIs(covs, "matrix")){
	  // FIXME
	  stop("Covariates must be supplied as a data.frame.");
	} 
      } else {
	op->ncov = 0;
        // int *par_cov;
        op->do_par_cov = 0;
      }
    } else {
      // data.frame or matrix
      stop("single solver only for now.");
    }
    CharacterVector sigmaN;
    if (!thetaMat.isNull() || !omega.isNull() || !sigma.isNull()){
      // Simulated Variable
      if (!rxIs(par1, "numeric")){
	stop("When specifying 'thetaMat', 'omega', or 'sigma' the parameters cannot be a data.frame/matrix.");
      }
      unsigned int nObs = nobs + (addDosing ? ndoses : 0);
      unsigned int nSub0 = nSub;
      if (nSub == 1){
	nSub0 = nsub;
      } else if (nSub > 1 && nsub > 1 &&  nSub != nsub){
        stop("You provided multi-subject data and asked to simulate a different number of subjects;  I don't know what to do.");
      }
      par1 =  as<RObject>(rxSimThetaOmega(as<Nullable<NumericVector>>(par1), omega, omegaDf, omegaIsChol, nSub0, thetaMat, thetaDf, thetaIsChol, nStud,
					  sigma, sigmaDf, sigmaIsChol, nObs, nCoresRV, simVariability));
      // The parameters are in the same format as they would be if they were specified as part of the original dataset.
    }
    // .sigma could be reassigned in an update, so check outside simulation function.
    if (_rxModels.exists(".sigma")){
      sigmaN = as<CharacterVector>((as<List>((as<NumericMatrix>(_rxModels[".sigma"])).attr("dimnames")))[1]);
    }
    int parType = 1;
    NumericVector parNumeric;
    DataFrame parDf;
    NumericMatrix parMat;
    CharacterVector nmP;
    int nPopPar = 1;
    if (!theta.isNULL() || !eta.isNULL()){
      par1 = rxSetupParamsThetaEta(par1, theta, eta);
    }
    if (rxIs(par1, "numeric") || rxIs(par1, "integer")){
      parNumeric = as<NumericVector>(par1);
      if (parNumeric.hasAttribute("names")){
	nmP = parNumeric.names();
      } else if (parNumeric.size() == pars.size()) {
	nmP = pars;
      } else {
	stop("If parameters are not named, they must match the order and size of the parameters in the model.");
      }
    } else if (rxIs(par1, "data.frame")){
      parDf = as<DataFrame>(par1);
      parType = 2;
      nmP = parDf.names();
      nPopPar = parDf.nrows();
    } else if (rxIs(par1, "matrix")){
      parMat = as<NumericMatrix>(par1);
      nPopPar = parMat.nrow();
      parType = 3;
      if (parMat.hasAttribute("dimnames")){
	Nullable<CharacterVector> colnames0 = as<Nullable<CharacterVector>>((as<List>(parMat.attr("dimnames")))[1]);
	if (colnames0.isNull()){
	  if (parMat.ncol() == pars.size()){
	    nmP = pars;
	  } else {
            stop("If parameters are not named, they must match the order and size of the parameters in the model.");
          }
        } else {
	  nmP = CharacterVector(colnames0);
	}
      } else if (parMat.ncol() == pars.size()) {
	nmP = pars;
      } else {
        stop("If parameters are not named, they must match the order and size of the parameters in the model.");
      }
    } 
    // Make sure the user input all the parmeters.
    gParPosSetup(npars);
    std::string errStr = "";
    bool allPars = true;
    bool curPar = false;
    CharacterVector mvIniN = mvIni.names();
    gsvarSetup(sigmaN.size());
    for (i = npars; i--;){
      curPar = false;
      // Check to see if this is a covariate.
      for (j = op->ncov; j--;){
	if (gpar_cov[j] == (int)(i + 1)){
	  gParPos[i] = 0; // These are set at run-time and "dont" matter.
	  curPar = true;
	  break;
	}
      }
      // Check for the sigma-style simulated parameters.
      if (!curPar){
	for (j = sigmaN.size(); j--;){
          if (sigmaN[j] == pars[i]){
	    gsvar[j] = i;
	    gParPos[i] = 0; // These are set at run-time and "dont" matter.
	    curPar = true;
	    break;
	  }
	}
      }
      // Next, check to see if this is a user-specified parameter
      if (!curPar){
        for (j = nmP.size(); j--;){
          if (nmP[j] == pars[i]){
            curPar = true;
            gParPos[i] = j + 1;
            break;
          }
        }
      }
      // last, check for $ini values
      if (!curPar){
        for (j = mvIniN.size(); j--;){
          if (mvIniN[j] == pars[i]){
            curPar = true;
            gParPos[i] = -j - 1;
            break;
          }
        }
      }
      if (!curPar){
        if (errStr == ""){
          errStr = "The following parameter(s) are required for solving: " + pars[i];
        } else {
          errStr = errStr + ", " + pars[i];
        }
        allPars = false;
      }
    }
    if (!allPars){
      CharacterVector modSyntax = mv["model"];
      Rcout << "Model:\n\n" + modSyntax[0] + "\n";
      stop(errStr);
    }
    op->svar = &gsvar[0];
    // Now setup the rest of the rx_solve object
    if (nPopPar != 1 && nPopPar % nsub != 0){
      stop("The number of parameters solved by RxODE for multi-subject data needs to be a multiple of the number of subjects.");
    }
    //
    double *gInfusionRate = global_InfusionRate(op->neq*nsub*nPopPar);
    std::fill_n(&gInfusionRate[0], op->neq*nsub*nPopPar, 0.0);

    gBadDoseSetup(op->neq*nsub*nPopPar);
    std::fill_n(&gBadDose[0], op->neq*nsub*nPopPar, 0);
    
    glhsSetup(lhs.size()*nsub*nPopPar);
    
    grcSetup(nsub*nPopPar);
    std::fill_n(&grc[0], nsub*nPopPar, 0);
    
    gsolveSetup((ndoses+nobs)*state.size()*nPopPar);
    std::fill_n(&gsolve[0],(ndoses+nobs)*state.size()*nPopPar,0.0);
    int curEvent = 0;
    
    switch(parType){
    case 1: // NumericVector
      if (nPopPar != 1) stop("Something is wrong... nPopPar != 1 but parameters are specified as a NumericVector.");
      gparsSetup(npars);
      for (i = npars; i--;){
	if (gParPos[i] == 0){ // Covariate or simulated variable.
	  gpars[i] = 0;//NA_REAL;
	} else if (gParPos[i] > 0){ // User specified parameter
	  gpars[i] = parNumeric[gParPos[i]-1];
	} else { // ini specified parameter.
	  gpars[i] = mvIni[-gParPos[i]-1];
	}
      }
      for (i = nsub; i--;){
	ind = &(rx->subjects[i]);
	ind->par_ptr = &gpars[0];
	ind->InfusionRate = &gInfusionRate[op->neq*i];
        ind->BadDose = &gBadDose[op->neq*i];
        ind->nBadDose = 0;
	// Hmas defined above.
	ind->tlast=0.0;
	ind->podo = 0.0;
	ind->ixds =  0;
	ind->sim = i+1;
	ind->solve = &gsolve[curEvent];
        curEvent += op->neq*ind->n_all_times;
        ind->lhs = &glhs[i*lhs.size()];
	ind->rc = &grc[i];
      }
      rx->nsub= nsub;
      rx->nsim = nsim;
      break;
    case 2: // DataFrame
      // Convert to NumericMatrix
      {
	unsigned int parDfi = parDf.size();
	parMat = NumericMatrix(nPopPar,parDfi);
	while (parDfi--){
	  parMat(_,parDfi)=NumericVector(parDf[parDfi]);
	}
      }
    case 3: // NumericMatrix
      gparsSetup(npars*nPopPar);
      for (i = npars; i--;){
	j = floor(i / npars);
        k = i % npars;
	if (gParPos[k] == 0){
          gpars[i] = 0;
        } else if (gParPos[k] > 0){
          // posPar[i] = j + 1;
          gpars[i] = parMat(j, gParPos[k]-1);
        } else {
          // posPar[i] = -j - 1;
          gpars[i] = mvIni[-gParPos[k]-1];
        }
      }
      rx->nsim = nPopPar / nsub;
      rx->nsub= nsub;
      for (unsigned int simNum = rx->nsim; simNum--;){
        for (unsigned int id = rx->nsub; id--;){
          unsigned int cid = id+simNum*nSub;
	  ind = &(rx->subjects[cid]);
	  ind->par_ptr = &gpars[cid*npars];
	  ind->InfusionRate = &gInfusionRate[op->neq*cid];
          ind->nBadDose = 0;
          // Hmax defined above.
          ind->tlast=0.0;
          ind->podo = 0.0;
          ind->ixds =  0;
          ind->sim = simNum+1;
          ind->solve = &gsolve[curEvent];
          curEvent += op->neq*ind->n_all_times;
          ind->lhs = &glhs[cid*lhsSize];
          ind->rc = &grc[cid];
	  if (simNum){
	    // Assign the pointers to the shared data
	    rx_solving_options_ind* indS = &(rx->subjects[id]);
	    if (op->do_par_cov){
              ind->cov_ptr = indS->cov_ptr;
	    }
	    ind->n_all_times = indS->n_all_times;
	    ind->HMAX = indS->HMAX;
	    ind->idose = indS->idose;
            ind->ndoses = indS->ndoses;
	    ind->dose = indS->dose;
	    ind->evid = indS->evid;
	    ind->all_times = indS->all_times;
	    ind->slvr_counter   = 0;
            ind->dadt_counter   = 0;
            ind->jac_counter    = 0;
            ind->id=id+1;
          }
        }
      }
      break;
    default: 
      stop("Something is wrong here.");
    }
    par_solve(rx);
    if (op->abort){
      stop("Aborted solve.");
    }
    int doDose = 0;
    if (addDosing){
      doDose = 1;
    } else {
      doDose = 0;
    }
    IntegerVector si = mv["state.ignore"];
    rx->stateIgnore = &si[0];
    List dat = RxODE_df(doDose);
    dat.attr("class") = CharacterVector::create("data.frame");
    List xtra;
    if (!rx->matrix) xtra = RxODE_par_df();
    int nr = as<NumericVector>(dat[0]).size();
    int nc = dat.size();
    if (rx->matrix){
      getRxModels();
      if(_rxModels.exists(".sigma")){
  	_rxModels.remove(".sigma");
      }
      if(_rxModels.exists(".sigmaL")){
  	_rxModels.remove(".sigmaL");
      }
      if(_rxModels.exists(".omegaL")){
  	_rxModels.remove(".omegaL");
      }
      if(_rxModels.exists(".theta")){
  	_rxModels.remove(".theta");
      }
      dat.attr("class") = "data.frame";
      NumericMatrix tmpM(nr,nc);
      for (int i = 0; i < dat.size(); i++){
  	tmpM(_,i) = as<NumericVector>(dat[i]);
      }
      tmpM.attr("dimnames") = List::create(R_NilValue,dat.names());
      return tmpM;
    } else {
      Function newEnv("new.env", R_BaseNamespace);
      Environment RxODE("package:RxODE");
      Environment e = newEnv(_["size"] = 29, _["parent"] = RxODE);
      getRxModels();
      if(_rxModels.exists(".theta")){
  	e[".theta"] = as<NumericMatrix>(_rxModels[".theta"]);
  	_rxModels.remove(".theta");
      }
      if(_rxModels.exists(".sigma")){
  	e[".sigma"] = as<NumericMatrix>(_rxModels[".sigma"]);
  	_rxModels.remove(".sigma");
      }
      if(_rxModels.exists(".omegaL")){
  	e[".omegaL"] = as<List>(_rxModels[".omegaL"]);
  	_rxModels.remove(".omegaL");
      }
      if(_rxModels.exists(".sigmaL")){
  	e[".sigmaL"] = as<List>(_rxModels[".sigmaL"]);
  	_rxModels.remove(".sigmaL");
      }
      e["check.nrow"] = nr;
      e["check.ncol"] = nc;
      e["check.names"] = dat.names();
      // Save information
      // Remove one final; Just for debug.
      // e["parData"] = parData;
      List pd = as<List>(xtra[0]);
      if (pd.size() == 0){
  	e["params.dat"] = R_NilValue;
      } else {
  	e["params.dat"] = pd;
      }
      if (rx->nsub == 1 && rx->nsim == 1){
  	int n = pd.size();
  	NumericVector par2(n);
  	for (int i = 0; i <n; i++){
  	  par2[i] = (as<NumericVector>(pd[i]))[0];
  	}
  	par2.names() = pd.names();
  	if (par2.size() == 0){
  	  e["params.single"] = R_NilValue;
  	} else {
  	  e["params.single"] = par2;
  	}
      } else {
  	e["params.single"] = R_NilValue;
      }
      e["EventTable"] = xtra[1];
      e["dosing"] = xtra[3];
      e["sampling"] = xtra[2];
      e["obs.rec"] = xtra[4];
      e["covs"] = xtra[5];
      e["counts"] = xtra[6];
      e["inits.dat"] = initsC;
      CharacterVector units = CharacterVector::create(amountUnits[0], timeUnits[0]);
      units.names() = CharacterVector::create("dosing","time");
      e["units"] = units;
      e["nobs"] = rx->nobs;
    
      Function eventTable("eventTable",RxODE);
      List et = eventTable(_["amount.units"] = amountUnits, _["time.units"] = timeUnits);
      Function importEt = as<Function>(et["import.EventTable"]);
      importEt(e["EventTable"]);
      e["events.EventTable"] = et;
      e["args.object"] = object;
      e["dll"] = rxDll(object);
      if (!swappedEvents){
  	e["args.params"] = params;    
  	e["args.events"] = events;
      } else {
  	e["args.params"] = events;    
  	e["args.events"] = params;
      }
      e["args.inits"] = inits;
      e["args.covs"] = covs;
      e["args.method"] = method;
      e["args.transit_abs"] = transit_abs;
      e["args.atol"] = atol;
      e["args.rtol"] = rtol;
      e["args.maxsteps"] = maxsteps;
      e["args.hmin"] = hmin;
      e["args.hmax"] = hmax;
      e["args.hini"] = hini;
      e["args.maxordn"] = maxordn;
      e["args.maxords"] = maxords;
      e["args.cores"] = cores;
      e["args.covs_interpolation"] = covs_interpolation;
      e["args.addCov"] = addCov;
      e["args.matrix"] = matrix;
      e["args.sigma"] = sigma;
      e["args.sigmaDf"] = sigmaDf;
      e["args.nCoresRV"] = nCoresRV;
      e["args.sigmaIsChol"] = sigmaIsChol;
      e["args.nDisplayProgress"] = nDisplayProgress;
      e["args.amountUnits"] = amountUnits;
      e["args.timeUnits"] = timeUnits;
      e["args.addDosing"] = addDosing;
      e[".real.update"] = true;
      CharacterVector cls= CharacterVector::create("rxSolve", "data.frame");
      cls.attr(".RxODE.env") = e;    
      dat.attr("class") = cls;
      return(dat);
    }
  }
  return R_NilValue;
}


//[[Rcpp::export]]
SEXP rxSolveCsmall(const RObject &object,
                   const Nullable<CharacterVector> &specParams = R_NilValue,
                   const Nullable<List> &extraArgs = R_NilValue,
                   const RObject &params = R_NilValue,
                   const RObject &events = R_NilValue,
                   const RObject &inits = R_NilValue,
		   const RObject &scale = R_NilValue,
                   const RObject &covs  = R_NilValue,
                   const Nullable<List> &optsL = R_NilValue){
  if (optsL.isNull()){
    stop("Not meant to be called directly.  Needs options setup.");
  }
  List opts = List(optsL);
  return rxSolveC(object, specParams, extraArgs, params, events, inits, scale, covs,
                  (int)opts[0], // const int method = 2,
                  as<Nullable<LogicalVector>>(opts[1]), // const Nullable<LogicalVector> &transit_abs = R_NilValue,
                  (double)opts[2], //const double atol = 1.0e-8,
                  (double)opts[3], // const double rtol = 1.0e-6,
                  (int)opts[4], //const int maxsteps = 5000,
                  (double)opts[5], //const double hmin = 0,
                  as<Nullable<NumericVector>>(opts[6]), //const Nullable<NumericVector> &hmax = R_NilValue,
                  (double)opts[7], //const double hini = 0,
                  (int)opts[8], //const int maxordn = 12,
                  opts[9], //const int maxords = 5,
                  opts[10], //const int cores = 1,
                  opts[11], //const int covs_interpolation = 0,
                  opts[12], //bool addCov = false,
                  opts[13], //bool matrix = false,
                  opts[14], //const RObject &sigma= R_NilValue,
                  opts[15], //const RObject &sigmaDf= R_NilValue,
                  opts[16], //const int &nCoresRV= 1,
                  opts[17], //const bool &sigmaIsChol= false,
                  opts[18], // nDisplayProgress
                  opts[19], //const CharacterVector &amountUnits = NA_STRING,
                  opts[20], //const CharacterVector &timeUnits = "hours",
                  opts[21], //const bool updateObject = false
                  opts[22], //const RObject &eta = R_NilValue,
                  opts[23], //const bool addDosing = false
		  opts[24], // 
		  opts[25],
		  opts[26], // const RObject &omega = R_NilValue, 
		  opts[27], // const RObject &omegaDf = R_NilValue, 
                  opts[28], // const bool &omegaIsChol = false,
                  opts[29], // const int nSub = 1, 
                  opts[30], // const RObject &thetaMat = R_NilValue, 
                  opts[31], // const RObject &thetaDf = R_NilValue, 
                  opts[32], // const bool &thetaIsChol = false,
                  opts[33], // const int nStud = 1, 
                  opts[34] // const bool simVariability=true
		  );
}

//[[Rcpp::export]]
RObject rxSolveGet(RObject obj, RObject arg, LogicalVector exact = true){
  std::string sarg;
  int i, j, n;
  if (rxIs(obj, "data.frame")){
    List lst = as<List>(obj);
    if (rxIs(arg, "character")){
      sarg = as<std::string>(arg);
      CharacterVector nm = lst.names();
      n = nm.size();
      unsigned int slen = strlen(sarg.c_str());
      int dexact = -1;
      if (exact[0] == TRUE){
	dexact = 1;
      } else if (exact[0] == FALSE){
	dexact = 0;
      }
      unsigned int slen2;
      for (i = 0; i < n; i++){
	slen2 = strlen((as<std::string>(nm[i])).c_str());
	if (slen <= slen2 &&
	    (strncmp((as<std::string>(nm[i])).c_str(), sarg.c_str(), slen)  == 0 ) &&
	    (dexact != 1 || (dexact == 1 && slen == slen2))){
	  if (dexact == -1){
	    warning("partial match of '%s' to '%s'",sarg.c_str(), (as<std::string>(nm[i])).c_str());
	  }
	  return lst[i];
	}
      }
      if (rxIs(obj, "rxSolve")){
	rxCurObj = obj;
	CharacterVector cls = lst.attr("class");
	Environment e = as<Environment>(cls.attr(".RxODE.env"));
	if (sarg == "env"){
	  return as<RObject>(e);
	}
	if (sarg == "model"){
	  List mv = rxModelVars(obj);
	  CharacterVector mods = mv["model"];
	  CharacterVector retS = CharacterVector::create(mods["model"]);
	  retS.attr("class") = "RxODE.modeltext";
	  return(retS);
	}
	if (!e.exists("get.EventTable")){
          Function parse2("parse", R_BaseNamespace);
          Function eval2("eval", R_BaseNamespace);
          // eventTable style methods
          e["get.EventTable"] = eval2(_["expr"]   = parse2(_["text"]="function() EventTable"),
                                       _["envir"]  = e);
          e["get.obs.rec"] = eval2(_["expr"]   = parse2(_["text"]="function() obs.rec"),
                                    _["envir"]  = e);
          e["get.nobs"] = eval2(_["expr"]   = parse2(_["text"]="function() nobs"),
                                 _["envir"]  = e);
          e["add.dosing"] = eval2(_["expr"]   = parse2(_["text"]="function(...) {et <- create.eventTable(); et$add.dosing(...); invisible(rxSolve(args.object,events=et,update.object=TRUE))}"),
                                   _["envir"]  = e);
          e["clear.dosing"] = eval2(_["expr"]   = parse2(_["text"]="function(...) {et <- create.eventTable(); et$clear.dosing(...); invisible(rxSolve(args.object,events=et,update.object=TRUE))}"),
                                     _["envir"]  = e);
          e["get.dosing"] = eval2(_["expr"]   = parse2(_["text"]="function() dosing"),
                                   _["envir"]  = e);

          e["add.sampling"] = eval2(_["expr"]   = parse2(_["text"]="function(...) {et <- create.eventTable(); et$add.sampling(...); invisible(rxSolve(args.object,events=et,update.object=TRUE))}"),
                                     _["envir"]  = e);
      
          e["clear.sampling"] = eval2(_["expr"]   = parse2(_["text"]="function(...) {et <- create.eventTable(); et$clear.sampling(...); invisible(rxSolve(args.object,events=et,update.object=TRUE))}"),
                                       _["envir"]  = e);

          e["replace.sampling"] = eval2(_["expr"]   = parse2(_["text"]="function(...) {et <- create.eventTable(); et$clear.sampling(); et$add.sampling(...); invisible(rxSolve(args.object,events=et,update.object=TRUE))}"),
                                         _["envir"]  = e);

          e["get.sampling"] = eval2(_["expr"]   = parse2(_["text"]="function() sampling"),
                                     _["envir"]  = e);
      
          e["get.units"] = eval2(_["expr"]   = parse2(_["text"]="function() units"),
                                  _["envir"]  = e);

          e["import.EventTable"] = eval2(_["expr"]   = parse2(_["text"]="function(imp) {et <- create.eventTable(imp); invisible(rxSolve(args.object,events=et,update.object=TRUE))}"),
					 _["envir"]  = e);
      
          e["create.eventTable"] = eval2(_["expr"]   = parse2(_["text"]="function(new.event) {et <- eventTable(amount.units=units[1],time.units=units[2]);if (missing(new.event)) {nev <- EventTable; } else {nev <- new.event;}; et$import.EventTable(nev); return(et);}"),
                                          _["envir"]  = e);
          // Note event.copy doesn't really make sense...?  The create.eventTable does basically the same thing.
	}
        if (e.exists(sarg)){
          return e[sarg];
        }
        if (sarg == "params" || sarg == "par" || sarg == "pars" || sarg == "param"){
	  return e["params.dat"];
	} else if (sarg == "inits" || sarg == "init"){
	  return e["inits.dat"];
	} else if (sarg == "t"){
	  return lst["time"];
	} else if (sarg == "sigma.list" && e.exists(".sigmaL")){
	  return e[".sigmaL"];
	} else if (sarg == "omega.list" && e.exists(".omegaL")){
          return e[".omegaL"];
	}
	// Now parameters
	List pars = List(e["params.dat"]);
	CharacterVector nmp = pars.names();
	n = pars.size();
	for (i = n; i--;){
	  if (nmp[i] == sarg){
	    return pars[sarg];
	  }
	}
	// // Now inis.
	// Function sub("sub", R_BaseNamespace);
	NumericVector ini = NumericVector(e["inits.dat"]);
	CharacterVector nmi = ini.names();
	n = ini.size();
        std::string cur;
        NumericVector retN(1);
        for (i = n; i--; ){
	  cur = as<std::string>(nmi[i]) + "0";
	  if (cur == sarg){
	    retN = ini[i];
	    return as<RObject>(retN);
	  }
	  cur = as<std::string>(nmi[i]) + "_0";
          if (cur == sarg){
	    retN = ini[i];
            return as<RObject>(retN);
          }
          cur = as<std::string>(nmi[i]) + ".0";
          if (cur == sarg){
            retN = ini[i];
            return as<RObject>(retN);
          }
          cur = as<std::string>(nmi[i]) + "[0]";
          if (cur == sarg){
	    retN = ini[i];
            return as<RObject>(retN);
          }
          cur = as<std::string>(nmi[i]) + "(0)";
          if (cur == sarg){
	    retN = ini[i];
	    return as<RObject>(retN);
          }
          cur = as<std::string>(nmi[i]) + "{0}";
          if (cur == sarg){
	    retN = ini[i];
            return as<RObject>(retN);
          }
	}
	List mv = rxModelVars(obj);
	CharacterVector normState = mv["normal.state"];
	CharacterVector parsC = mv["params"];
        CharacterVector lhsC = mv["lhs"];
	for (i = normState.size(); i--;){
	  for (j = parsC.size(); j--; ){
	    std::string test = "_sens_" + as<std::string>(normState[i]) + "_" + as<std::string>(parsC[j]);
	    if (test == sarg){
	      test = "rx__sens_" + as<std::string>(normState[i]) + "_BY_" + as<std::string>(parsC[j]) + "__";
	      return lst[test];
	    }
            test = as<std::string>(normState[i]) + "_" + as<std::string>(parsC[j]);
            if (test == sarg){
              test = "rx__sens_" + as<std::string>(normState[i]) + "_BY_" + as<std::string>(parsC[j]) + "__";
	      return lst[test];
	    }
            test = as<std::string>(normState[i]) + "." + as<std::string>(parsC[j]);
            if (test == sarg){
              test = "rx__sens_" + as<std::string>(normState[i]) + "_BY_" + as<std::string>(parsC[j]) + "__";
              return lst[test];
            }
	  }
          for (j = lhsC.size(); j--;){
            std::string test = "_sens_" + as<std::string>(normState[i]) + "_" + as<std::string>(lhsC[j]);
            if (test == sarg){
              test = "rx__sens_" + as<std::string>(normState[i]) + "_BY_" + as<std::string>(lhsC[j]) + "__";
              return lst[test];
            }
            test = as<std::string>(normState[i]) + "_" + as<std::string>(lhsC[j]);
            if (test == sarg){
              test = "rx__sens_" + as<std::string>(normState[i]) + "_BY_" + as<std::string>(lhsC[j]) + "__";
              return lst[test];
            }
            test = as<std::string>(normState[i]) + "." + as<std::string>(lhsC[j]);
            if (test == sarg){
              test = "rx__sens_" + as<std::string>(normState[i]) + "_BY_" + as<std::string>(lhsC[j]) + "__";
              return lst[test];
            }
          }
	}
      }
    } else {
      if (rxIs(arg, "integer") || rxIs(arg, "numeric")){
	int iarg = as<int>(arg);
	if (iarg < lst.size()){
	  return lst[iarg-1];
	}
      }
    }
  }
  return R_NilValue;
}

//[[Rcpp::export]]
RObject rxSolveUpdate(RObject obj,
		      RObject arg = R_NilValue,
		      RObject value = R_NilValue){
  if (rxIs(obj,"rxSolve")){
    rxCurObj = obj;
    if (rxIs(arg,"character")){
      CharacterVector what = CharacterVector(arg);
      if (what.size() == 1){
	std::string sarg = as<std::string>(what[0]);
	// Now check to see if this is something that can be updated...
	if (sarg == "params"){
	  return rxSolveC(obj,
                          CharacterVector::create("params"),
			  R_NilValue,
                          value, //defrx_params,
                          defrx_events,
                          defrx_inits,
			  R_NilValue, // scale (cannot be updated currently.)
                          defrx_covs,
                          defrx_method,
                          defrx_transit_abs,
                          defrx_atol,
                          defrx_rtol,
                          defrx_maxsteps,
                          defrx_hmin,
                          defrx_hmax,
                          defrx_hini,
                          defrx_maxordn,
                          defrx_maxords,
                          defrx_cores,
                          defrx_covs_interpolation,
                          defrx_addCov,
                          defrx_matrix,
                          defrx_sigma,
                          defrx_sigmaDf,
                          defrx_nCoresRV,
                          defrx_sigmaIsChol,
                          defrx_nDisplayProgress,
                          defrx_amountUnits,
                          defrx_timeUnits, defrx_addDosing);
	} else if (sarg == "events"){
	  return rxSolveC(obj,
			  CharacterVector::create("events"),
			  R_NilValue,
			  defrx_params,
			  value, // defrx_events,
			  defrx_inits,
			  R_NilValue, // scale
			  defrx_covs,
			  defrx_method,
			  defrx_transit_abs,
			  defrx_atol,
			  defrx_rtol,
			  defrx_maxsteps,
			  defrx_hmin,
			  defrx_hmax,
			  defrx_hini,
			  defrx_maxordn,
			  defrx_maxords,
			  defrx_cores,
			  defrx_covs_interpolation,
			  defrx_addCov,
			  defrx_matrix,
			  defrx_sigma,
			  defrx_sigmaDf,
			  defrx_nCoresRV,
			  defrx_sigmaIsChol,
                          defrx_nDisplayProgress,
			  defrx_amountUnits,
			  defrx_timeUnits, 
			  defrx_addDosing);
	} else if (sarg == "inits"){
	  return rxSolveC(obj,
                          CharacterVector::create("inits"),
			  R_NilValue,
                          defrx_params,
                          defrx_events,
                          as<RObject>(value), //defrx_inits,
			  R_NilValue, // scale
			  defrx_covs,
                          defrx_method,
                          defrx_transit_abs,
                          defrx_atol,
                          defrx_rtol,
                          defrx_maxsteps,
                          defrx_hmin,
                          defrx_hmax,
                          defrx_hini,
                          defrx_maxordn,
                          defrx_maxords,
                          defrx_cores,
                          defrx_covs_interpolation,
                          defrx_addCov,
                          defrx_matrix,
                          defrx_sigma,
                          defrx_sigmaDf,
                          defrx_nCoresRV,
                          defrx_sigmaIsChol,
                          defrx_nDisplayProgress,
                          defrx_amountUnits,
                          defrx_timeUnits, 
			  defrx_addDosing);
	} else if (sarg == "covs"){
	  return rxSolveC(obj,
                          CharacterVector::create("covs"),
			  R_NilValue,
                          defrx_params,
                          defrx_events,
                          defrx_inits,
			  R_NilValue,
                          value,// defrx_covs,
                          defrx_method,
                          defrx_transit_abs,
                          defrx_atol,
                          defrx_rtol,
                          defrx_maxsteps,
                          defrx_hmin,
                          defrx_hmax,
                          defrx_hini,
                          defrx_maxordn,
                          defrx_maxords,
                          defrx_cores,
                          defrx_covs_interpolation,
                          defrx_addCov,
                          defrx_matrix,
                          defrx_sigma,
                          defrx_sigmaDf,
                          defrx_nCoresRV,
                          defrx_sigmaIsChol,
                          defrx_nDisplayProgress,
                          defrx_amountUnits,
                          defrx_timeUnits, 
			  defrx_addDosing);
	} else if (sarg == "t" || sarg == "time"){
	  CharacterVector classattr = obj.attr("class");
          Environment e = as<Environment>(classattr.attr(".RxODE.env"));
	  Function f = as<Function>(e["replace.sampling"]);
	  return f(value);
        } else {
	  CharacterVector classattr = obj.attr("class");
	  Environment e = as<Environment>(classattr.attr(".RxODE.env"));
	  List pars = List(e["params.dat"]);
	  CharacterVector nmp = pars.names();
	  int i, n, np, nc, j;
	  np = (as<NumericVector>(pars[0])).size();
	  RObject covsR = e["covs"];
	  List covs;
	  if (!covsR.isNULL()){
	    covs = List(covsR);
	  }
	  CharacterVector nmc;
	  if (covs.hasAttribute("names")){
	    nmc = covs.names();
	    nc = (as<NumericVector>(covs[0])).size();
	  } else {
	    nc = as<int>(e["nobs"]);
	  }
	  //////////////////////////////////////////////////////////////////////////////
	  // Update Parameters by name
	  n = pars.size();
	  for (i = n; i--; ){
	    if (nmp[i] == sarg){
	      // Update solve.
	      NumericVector val = NumericVector(value);
	      if (val.size() == np){
		// Update Parameter
		pars[i] = val;
		return rxSolveC(obj,
				CharacterVector::create("params"),
				R_NilValue,
				pars, //defrx_params,
				defrx_events,
				defrx_inits,
				R_NilValue,
				defrx_covs,
				defrx_method,
				defrx_transit_abs,
				defrx_atol,
				defrx_rtol,
				defrx_maxsteps,
				defrx_hmin,
				defrx_hmax,
				defrx_hini,
				defrx_maxordn,
				defrx_maxords,
				defrx_cores,
				defrx_covs_interpolation,
				defrx_addCov,
				defrx_matrix,
				defrx_sigma,
				defrx_sigmaDf,
				defrx_nCoresRV,
				defrx_sigmaIsChol,
                                defrx_nDisplayProgress,
				defrx_amountUnits,
				defrx_timeUnits, 
				defrx_addDosing);
	      } else if (val.size() == nc){
		// Change parameter -> Covariate
		List newPars(pars.size()-1);
		CharacterVector newParNames(pars.size()-1);
		for (j = i; j--;){
		  newPars[j]     = pars[j];
		  newParNames[j] = nmp[j];
		}
		for (j=i+1; j < pars.size(); j++){
		  newPars[j-1]     = pars[j];
		  newParNames[j-1] = nmp[j];
		}
		newPars.attr("names") = newParNames;
		newPars.attr("class") = "data.frame";
		newPars.attr("row.names") = IntegerVector::create(NA_INTEGER,-np);
		List newCovs(covs.size()+1);
		CharacterVector newCovsNames(covs.size()+1);
		for (j = covs.size(); j--;){
		  newCovs[j]      = covs[j];
		  newCovsNames[j] = nmc[j];
		}
		j = covs.size();
		newCovs[j]      = val;
		newCovsNames[j] = nmp[i];
		newCovs.attr("names") = newCovsNames;
		newCovs.attr("class") = "data.frame";
		newCovs.attr("row.names") = IntegerVector::create(NA_INTEGER,-nc);
		return rxSolveC(obj,
				CharacterVector::create("params","covs"),
				R_NilValue,
				newPars, //defrx_params,
				defrx_events,
				defrx_inits,
				R_NilValue,
				newCovs, //defrx_covs
				defrx_method,
				defrx_transit_abs,
				defrx_atol,
				defrx_rtol,
				defrx_maxsteps,
				defrx_hmin,
				defrx_hmax,
				defrx_hini,
				defrx_maxordn,
				defrx_maxords,
				defrx_cores,
				defrx_covs_interpolation,
				defrx_addCov,
				defrx_matrix,
				defrx_sigma,
				defrx_sigmaDf,
				defrx_nCoresRV,
				defrx_sigmaIsChol,
                                defrx_nDisplayProgress,
				defrx_amountUnits,
				defrx_timeUnits, 
				defrx_addDosing);
	      }
	      return R_NilValue;
	    }
	  }
	  ///////////////////////////////////////////////////////////////////////////////
	  // Update Covariates by covariate name
	  n = covs.size();
	  for (i = n; i--;){
	    if (nmc[i] == sarg){
	      // Update solve.
	      NumericVector val = NumericVector(value);
	      if (val.size() == nc){
		// Update Covariate
		covs[i]=val;
		return rxSolveC(obj,
				CharacterVector::create("covs"),
				R_NilValue,
				defrx_params,
				defrx_events,
				defrx_inits,
				R_NilValue,
				covs, // defrx_covs,
				defrx_method,
				defrx_transit_abs,
				defrx_atol,
				defrx_rtol,
				defrx_maxsteps,
				defrx_hmin,
				defrx_hmax,
				defrx_hini,
				defrx_maxordn,
				defrx_maxords,
				defrx_cores,
				defrx_covs_interpolation,
				defrx_addCov,
				defrx_matrix,
				defrx_sigma,
				defrx_sigmaDf,
				defrx_nCoresRV,
				defrx_sigmaIsChol,
                                defrx_nDisplayProgress,
				defrx_amountUnits,
				defrx_timeUnits, 
				defrx_addDosing);
	      } else if (val.size() == np){
		// Change Covariate -> Parameter
		List newPars(pars.size()+1);
		CharacterVector newParNames(pars.size()+1);
		for (j = 0; j < pars.size(); j++){
		  newPars[j]     = pars[j];
		  newParNames[j] = nmp[j];
		}
		newPars[j]     = val;
		newParNames[j] = nmc[i];
		newPars.attr("names") = newParNames;
		newPars.attr("class") = "data.frame";
		newPars.attr("row.names") = IntegerVector::create(NA_INTEGER,-np);
		// if ()
		List newCovs(covs.size()-1);
		CharacterVector newCovsNames(covs.size()-1);
		for (j = 0; j < i; j++){
		  newCovs[j]      = covs[j];
		  newCovsNames[j] = nmc[j];
		}
		for (j=i+1; j < covs.size(); j++){
		  newCovs[j-1]      = covs[j];
		  newCovsNames[j-1] = nmc[j];
		}
		newCovs.attr("names") = newCovsNames;
		newCovs.attr("class") = "data.frame";
		newCovs.attr("row.names") = IntegerVector::create(NA_INTEGER,-nc);
		return rxSolveC(obj,
				CharacterVector::create("covs", "params"),
				R_NilValue,
				newPars,//defrx_params,
				defrx_events,
				defrx_inits,
				R_NilValue,
				newCovs, // defrx_covs,
				defrx_method,
				defrx_transit_abs,
				defrx_atol,
				defrx_rtol,
				defrx_maxsteps,
				defrx_hmin,
				defrx_hmax,
				defrx_hini,
				defrx_maxordn,
				defrx_maxords,
				defrx_cores,
				defrx_covs_interpolation,
				defrx_addCov,
				defrx_matrix,
				defrx_sigma,
				defrx_sigmaDf,
				defrx_nCoresRV,
				defrx_sigmaIsChol,
                                defrx_nDisplayProgress,
				defrx_amountUnits,
				defrx_timeUnits, 
				defrx_addDosing);
	      }
	    }
	  }
	  ////////////////////////////////////////////////////////////////////////////////
          // Update Initial Conditions
	  NumericVector ini = NumericVector(e["inits.dat"]);
          CharacterVector nmi = ini.names();
          n = ini.size();
          std::string cur;
	  bool doIt = false;
          for (i = 0; i < n; i++){
            cur = as<std::string>(nmi[i]) + "0";
	    if (cur == sarg){
	      doIt = true;
	    } else {
              cur = as<std::string>(nmi[i]) + ".0";
              if (cur == sarg){
                doIt = true;
	      } else {
		cur = as<std::string>(nmi[i]) + "_0";
                if (cur == sarg){
		  doIt = true;
		} else {
		  cur = as<std::string>(nmi[i]) + "(0)";
                  if (cur == sarg){
                    doIt = true;
                  } else {
		    cur = as<std::string>(nmi[i]) + "[0]";
                    if (cur == sarg){
		      doIt = true;
		    } 
		  }
		}
	      }
	    }
	    if (doIt){
	      cur=as<std::string>(nmi[i]);
              NumericVector ini = NumericVector(e["inits.dat"]);
	      double v = as<double>(value);
	      for (j = 0; j < n; j++){
		if (cur == as<std::string>(nmi[j])){
		  ini[j] = v;
		}
	      }
              return rxSolveC(obj,
			      CharacterVector::create("inits"),
			      R_NilValue,
			      defrx_params,
			      defrx_events,
			      ini,
			      R_NilValue,
			      defrx_covs,
			      defrx_method,
			      defrx_transit_abs,
			      defrx_atol,
			      defrx_rtol,
			      defrx_maxsteps,
			      defrx_hmin,
			      defrx_hmax,
			      defrx_hini,
			      defrx_maxordn,
			      defrx_maxords,
			      defrx_cores,
			      defrx_covs_interpolation,
			      defrx_addCov,
			      defrx_matrix,
			      defrx_sigma,
			      defrx_sigmaDf,
			      defrx_nCoresRV,
			      defrx_sigmaIsChol,
                              defrx_nDisplayProgress,
			      defrx_amountUnits,
			      defrx_timeUnits, 
			      defrx_addDosing);
	    }
	  }
	  return R_NilValue;
	}
      }
    }
  }
  return R_NilValue;
}

extern "C" SEXP rxGetModelLib(const char *s){
  std::string str(s);
  getRxModels();
  if (_rxModels.exists(str)){
    return wrap(_rxModels.get(str));
  } else {
    return R_NilValue;
  }
}

//[[Rcpp::export]]
void rxRmModelLib_(std::string str){
  getRxModels();
  if (_rxModels.exists(str)){
    List trans =(as<List>(as<List>(_rxModels[str]))["trans"]);
    std::string rxlib = as<std::string>(trans["prefix"]);
    _rxModels.remove(str);
    if (_rxModels.exists(rxlib)){
      _rxModels.remove(rxlib);
    }
  }  
}
extern "C" void rxClearFuns();
extern "C" void rxRmModelLib(const char* s){
  std::string str(s);
  rxClearFuns();
  rxRmModelLib_(str);
}

Nullable<Environment> rxRxODEenv(RObject obj){
  if (rxIs(obj, "RxODE")){
    return(as<Environment>(obj));
  } else if (rxIs(obj, "rxSolve")){
    CharacterVector cls = obj.attr("class");
    Environment e = as<Environment>(cls.attr(".RxODE.env"));
    return rxRxODEenv(as<RObject>(e["args.object"]));
  } else if (rxIs(obj, "rxModelVars")){
    List mv = as<List>(obj);
    CharacterVector trans = mv["trans"];
    getRxModels();
    std::string prefix = as<std::string>(trans["prefix"]);
    if (_rxModels.exists(prefix)){
      return as<Environment>(_rxModels[prefix]);
    } else {
      return R_NilValue;
    }
  } else {
    return rxRxODEenv(as<RObject>(rxModelVars(obj)));
  }
}

//' Get RxODE model from object
//' @param obj RxODE family of objects
//' @export
//[[Rcpp::export]]
RObject rxGetRxODE(RObject obj){
  Nullable<Environment> rxode1 = rxRxODEenv(obj);
  if (rxode1.isNull()){
    // FIXME compile if needed.
    stop("Can't figure out the RxODE object");
  } else {
    Environment e = as<Environment>(rxode1);
    e.attr("class") = "RxODE";
    return as<RObject>(e);
  }
}

bool rxVersion_b = false;
CharacterVector rxVersion_;

extern "C" const char *rxVersion(const char *what){
  std::string str(what);
  if (!rxVersion_b){
    Environment RxODE("package:RxODE");
    Function f = as<Function>(RxODE["rxVersion"]);
    rxVersion_ = f();
    rxVersion_b=true;
  }
  return (as<std::string>(rxVersion_[str])).c_str();
}

//' Checks if the RxODE object was built with the current build
//'
//' @inheritParams rxModelVars
//'
//' @return boolean indicating if this was built with current RxODE
//'
//' @export
//[[Rcpp::export]]
bool rxIsCurrent(RObject obj){
  List mv = rxModelVars(obj);
  if (mv.containsElementNamed("version")){
    CharacterVector version = mv["version"];
    const char* cVerC = rxVersion("md5");
    std::string str(cVerC);
    std::string str2 = as<std::string>(version["md5"]);
    if (str2 == str){
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

extern "C" void RxODE_assign_fn_pointers_(const char *mv);

//' Assign pointer based on model variables
//' @param object RxODE family of objects
//' @export
//[[Rcpp::export]]
void rxAssignPtr(SEXP object = R_NilValue){
  List mv=rxModelVars(as<RObject>(object));
  CharacterVector trans = mv["trans"];
    RxODE_assign_fn_pointers_((as<std::string>(trans["model_vars"])).c_str());
    rxUpdateFuns(as<SEXP>(trans));
    getRxSolve_();
    // Update rxModels environment.
    getRxModels();
  
    std::string ptr = as<std::string>(trans["model_vars"]); 
    if (!_rxModels.exists(ptr)){
      _rxModels[ptr] = mv;
    } else if (!rxIsCurrent(as<RObject>(_rxModels[ptr]))) {
      _rxModels[ptr] = mv;
    }
    Nullable<Environment> e1 = rxRxODEenv(object);
    if (!e1.isNull()){
      std::string prefix = as<std::string>(trans["prefix"]);
      if (!_rxModels.exists(prefix)){
        Environment e = as<Environment>(e1);
        _rxModels[prefix] = e;
      }
    }
}

extern "C" void rxAssignPtrC(SEXP obj){
  rxAssignPtr(obj);
}

//' Get the number of cores in a system
//' @export
//[[Rcpp::export]]
IntegerVector rxCores(){
  unsigned concurentThreadsSupported = std::thread::hardware_concurrency();
  return IntegerVector::create((int)(concurentThreadsSupported));
}

//' Return the DLL associated with the RxODE object
//'
//' This will return the dynamic load library or shared object used to
//' run the C code for RxODE.
//'
//' @param obj A RxODE family of objects or a character string of the
//'     model specification or location of a file with a model
//'     specification.
//'
//' @return a path of the library
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
std::string rxDll(RObject obj){
  if (rxIs(obj,"RxODE")){
    Environment e = as<Environment>(obj);
    return as<std::string>((as<List>(e["rxDll"]))["dll"]);
  } else if (rxIs(obj,"rxSolve")) {
    CharacterVector cls = obj.attr("class");
    Environment e = as<Environment>(cls.attr(".RxODE.env"));
    return(as<std::string>(e["dll"]));
  } else if (rxIs(obj, "rxDll")){
    return as<std::string>(as<List>(obj)["dll"]);
  } else if (rxIs(obj, "character")){
    Environment RxODE("package:RxODE");
    Function f = as<Function>(RxODE["rxCompile.character"]);
    RObject newO = f(as<std::string>(obj));
    return(rxDll(newO));
  } else {
    List mv = rxModelVars(obj);
    Nullable<Environment> en = rxRxODEenv(mv);
    if (en.isNull()){
      stop("Can't figure out the DLL for this object");
    } else {
      Environment e = as<Environment>(en);
      return as<std::string>((as<List>(e["rxDll"]))["dll"]);
    }
  }
}

//' Return the C file associated with the RxODE object
//'
//' This will return C code for generating the RxODE DLL.
//'
//' @param obj A RxODE family of objects or a character string of the
//'     model specification or location of a file with a model
//'     specification.
//'
//' @return a path of the library
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
CharacterVector rxC(RObject obj){
  std::string rets;
  CharacterVector ret(1);
  if (rxIs(obj,"RxODE")){
    Environment e = as<Environment>(obj);
    rets = as<std::string>((as<List>(e["rxDll"]))["c"]);
  } else if (rxIs(obj,"rxSolve")) {
    CharacterVector cls = obj.attr("class");
    Environment e = as<Environment>(cls.attr(".RxODE.env"));
    rets = as<std::string>(e["c"]);
  } else if (rxIs(obj, "rxDll")){
    rets = as<std::string>(as<List>(obj)["c"]);
  } else if (rxIs(obj, "character")){
    Environment RxODE("package:RxODE");
    Function f = as<Function>(RxODE["rxCompile.character"]);
    RObject newO = f(as<std::string>(obj));
    rets = rxDll(newO);
  } else {
    List mv = rxModelVars(obj);
    Nullable<Environment> en = rxRxODEenv(mv);
    if (en.isNull()){
      stop("Can't figure out the DLL for this object");
    } else {
      Environment e = as<Environment>(en);
      rets = as<std::string>((as<List>(e["rxDll"]))["dll"]);
    }
  }
  ret[0] = rets;
  ret.attr("class") = "rxC";
  return ret;
}

//' Determine if the DLL associated with the RxODE object is loaded
//'
//' @param obj A RxODE family of objects 
//'
//' @return Boolean returning if the RxODE library is loaded.
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
bool rxIsLoaded(RObject obj){
  if (obj.isNULL()) return false;
  Function isLoaded("is.loaded", R_BaseNamespace);
  List mv = rxModelVars(obj);
  CharacterVector trans = mv["trans"];
  std::string dydt = as<std::string>(trans["ode_solver"]);
  return as<bool>(isLoaded(dydt));
}

//' Load RxODE object
//'
//' @param obj A RxODE family of objects 
//'
//' @return Boolean returning if the RxODE library is loaded.
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
bool rxDynLoad(RObject obj){
  if (!rxIsLoaded(obj)){
    std::string file = rxDll(obj);
    if (fileExists(file)){
      Function dynLoad("dyn.load", R_BaseNamespace);
      dynLoad(file);
    } else {
      Nullable<Environment> e1 = rxRxODEenv(obj);
      if (!e1.isNull()){
	Environment e = as<Environment>(e1);
	Function compile = as<Function>(e["compile"]);
	compile();
      }
    }
  }
  if (rxIsLoaded(obj)){
    rxAssignPtr(obj);
    return true;
  } else {
    return false;
  }
}

//' Unload RxODE object
//'
//' @param obj A RxODE family of objects 
//'
//' @return Boolean returning if the RxODE library is loaded.
//'
//' @keywords internal
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
bool rxDynUnload(RObject obj){
  List mv = rxModelVars(obj);
  CharacterVector trans = mv["trans"];
  std::string ptr = as<std::string>(trans["model_vars"]);
  if (rxIsLoaded(obj)){
    Function dynUnload("dyn.unload", R_BaseNamespace);
    std::string file = rxDll(obj);
    dynUnload(file);
  } 
  rxRmModelLib_(ptr);
  return !(rxIsLoaded(obj));
}

//' Delete the DLL for the model
//'
//' This function deletes the DLL, but doesn't delete the model
//' information in the object.
//'
//' @param obj RxODE family of objects
//'
//' @return A boolean stating if the operation was successful.
//'
//' @author Matthew L.Fidler
//' @export
//[[Rcpp::export]]
bool rxDelete(RObject obj){
  std::string file = rxDll(obj);
  if (rxDynUnload(obj)){
    CharacterVector cfileV = rxC(obj);
    std::string cfile = as<std::string>(cfileV[0]);
    if (fileExists(cfile)) remove(cfile.c_str());
    if (!fileExists(file)) return true;
    if (remove(file.c_str()) == 0) return true;
  }
  return false;
}

extern "C" SEXP rxModelVarsC(char *ptr){
  // Rcout << "rxModelVars C: ";
  return rxGetFromChar(ptr, "");
}

extern "C" SEXP rxStateNames(char *ptr){
  // Rcout << "State: ";
  return rxGetFromChar(ptr, "state");
}

extern "C" SEXP rxLhsNames(char *ptr){
  // Rcout << "Lhs: ";
  return rxGetFromChar(ptr, "lhs");
}

extern "C" SEXP rxParamNames(char *ptr){
  // Rcout << "Param: ";
  return rxGetFromChar(ptr, "params");
}

extern "C" int rxIsCurrentC(SEXP obj){
  RObject robj = as<RObject>(obj);
  if (robj.isNULL()) return 0;
  bool ret = rxIsCurrent(robj);
  if (ret) return 1;
  return 0;
}


