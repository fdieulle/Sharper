% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/netSetStatic.R
\name{netSetStatic}
\alias{netSetStatic}
\title{Set static property value}
\usage{
netSetStatic(typeName, propertyName, value)
}
\arguments{
\item{typeName}{Full .Net type name}

\item{propertyName}{Property name to set value}

\item{value}{value to set.}
}
\description{
Sets a static property value from a .Net type name
}
\details{
Allows you to set a property value from .Net type name.
The input value will be converted from R type to a .Net type.
If the property value isn't a native C# type or a mapped conversion type you have to use an external pointer on .Net object.
You can define custom converters in C# for that see \code{RDotNetConverter} class.
}
\examples{
\dontrun{
library(sharper)

pkgPath <- path.package("sharper")
f <- file.path(pkgPath, "tests", "AssemblyForTests.dll")
netLoadAssembly(f)

type <- "AssemblyForTests.StaticClass"
netSetStatic(type, "DoubleProperty", 1.23)
netGetStatic(type, "DoubleProperty")

netSetStatic(type, "Int32Property", 123L)
netGetStatic(type, "Int32Property")

netSetStatic(type, "DoubleArrayProperty", c(1.23, 1.24, 1.25))
netGetStatic(type, "DoubleArrayProperty")

netSetStatic(type, "Int32ArrayProperty", c(123L, 124L, 125L))
netGetStatic(type, "Int32ArrayProperty")
}
}
