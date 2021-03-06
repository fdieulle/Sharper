#' @title 
#' Call static method
#' 
#' @description
#' Call a static .Net method for a given .Net type name
#'
#' @param typeName Full .Net type name 
#' @param methodName Method name to call
#' @param ... Method arguments
#' @param wrap Specify if you want to wrap `externalptr` .Net object into `NetObject` `R6` object. `FALSE` by default.
#' @param out_env In case of .Net method with `out` or `ref` argument, 
#' specify on which `environment` you want to out put this arguments. 
#' By default it's the caller `environment` i.e. `parent.frame()`.
#' @return Returns the .Net result. 
#' If a converter has been defined between the .Net type and a `R` type, the `R` type will be returned.
#' Otherwise an `externalptr` or a `NetObject` if `wrap` is set to `TRUE`.
#'
#'@details
#' Call a static method for a given .Net type name.
#' Ellipses has to keep the .net arguments method order, the named arguments are not supported yet.
#' If there is conflicts with a method name (many definition in .Net), a score is computed from your argument's 
#' order and type. We consider a higher score single value comparing to collection of values.
#' 
#' If you decide to set `wrap` to `TRUE`, the function returns a `NetObject` instead of a raw `externalptr`. 
#' To remind an `externalptr` is returned only if no one native converter has been found.
#' The `NetObject R6` object wrapper can be an inherited `R6` class. For more details about 
#' inherited `NetObject` class please see `netGenerateR6` function. 
#' 
#' The `out_env` is useful when the callee .Net method has some `out` or `ref` argument.
#' Because in .Net this argument set the given variable in the caller scope. We reflect this
#' mechanism in R. By default the given variable is modify in the parent `R environment` which means
#' the caller or `parent.frame()`. You can decide where to redirect the output value 
#' by specifying another `environment`. Of course be sure that the variable name exists in this 
#' targeted `environment`.
#'
#' @export
#' @examples
#' \dontrun{
#' library(sharper)
#' 
#' pkgPath <- path.package("sharper")
#' f <- file.path(pkgPath, "tests", "AssemblyForTests.dll")
#' netLoadAssembly(f)
#' 
#' type <- "AssemblyForTests.StaticClass"
#' netCallStatic(type, "CallWithInteger", 2L)
#' netCallStatic(type, "CallWithIntegerVector", c(2L, 3L))
#'
#' # Method selection single value vs vector values
#' netCallStatic(type, "SameMethodName", 1.23)
#' netCallStatic(type, "SameMethodName", c(1.24, 1.25))
#' netCallStatic(type, "SameMethodName", c(1.24, 1.25), 12L)
#' 
#' # wrap result
#' x <- NetObject$new(ptr = netNew("AssemblyForTests.DefaultCtorData"))
#' clone <- netCallStatic(type, "Clone", x, wrap = TRUE)
#' 
#' # out a variable
#' out_variable = 0
#' netCallStatic(type, "TryGetValue", out_variable)
#' }
netCallStatic <- function(typeName, methodName, ..., wrap = FALSE, out_env = parent.frame()) {
  
  if (any(as.logical(lapply(list(...), function(x) inherits(x, "NetObject"))))) {
    exp = substitute(list(...))
    if (length(exp) > 1) {
      src <- paste0(".External('rCallStaticMethod', '", typeName, "', '", methodName, "'")
      for (i in 2:length(exp)) {
        wrappedArg <- substitute(netUnwrap(x), list(x = exp[[i]]))
        src <- paste(sep = ", ", src, deparse(wrappedArg))
      }
      src <- paste(sep = ", ", src, "PACKAGE = 'sharper')")
      # print(src)
      results <- eval(parse(text = src), envir = parent.frame())
    } else {
      results <- .External("rCallStaticMethod", typeName, methodName, ..., PACKAGE = 'sharper')
    }
  } else {
    results <- .External("rCallStaticMethod", typeName, methodName, ..., PACKAGE = 'sharper')
  }
  
  if (wrap) results <- netWrap(results)
	
	if (length(results) > 1) {
		args <- lapply(eval(substitute(alist(...))), deparse)
		for (i in seq_along(args)) {
			assign(args[[i]], results[[i + 1]], envir = out_env)
		}
	} 
	
	return (results[[1]])
}
