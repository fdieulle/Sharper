% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/install_dotnet_core.R
\name{install_dotnet_core}
\alias{install_dotnet_core}
\title{Install .Net Core.}
\usage{
install_dotnet_core(
  channel = "LTS",
  version = "latest",
  installDir = NULL,
  architecture = NULL,
  runtime = "dotnet"
)
}
\arguments{
\item{channel}{Specifies the source channel for the installation. The possible values are:
* \code{Current} - Most current release.
* \code{LTS} - Long-Term Support channel (most current supported release).
* Two-part version in X.Y format representing a specific release (for example, \code{2.0} or \code{1.0}).
* Branch name. For example, \verb{release/2.0.0}, \verb{release/2.0.0-preview2}, or \code{master} (for nightly releases).
The default value is \code{LTS}. For more information on \href{https://www.microsoft.com/net/platform/support-policy#dotnet-core}{.NET support channels}, see the .NET Support Policy page.}

\item{version}{Represents a specific build version. The possible values are:
* \code{latest} - Latest build on the channel (used with the \code{channel} option).
* \code{coherent} - Latest coherent build on the channel; uses the latest stable package combination (used with Branch name \code{channel} options).
* Three-part version in X.Y.Z format representing a specific build version; supersedes the -Channel option. For example: 2.0.0-preview2-006120.
If not specified, version defaults to latest.}

\item{installDir}{Specifies the installation path. The directory is created if it doesn't exist. The default value is \%LocalAppData\%/Microsoft/dotnet.
Binaries are placed directly in this directory.}

\item{architecture}{Architecture of the .NET Core binaries to install. Possible values are \verb{<auto>}, \code{amd64}, \code{x64}, \code{x86}, \code{arm64}, and \code{arm}. The default value is \code{x86} and \code{x64}.}

\item{runtime}{Installs just the shared runtime, not the entire SDK. The possible values are:
* \code{dotnet} - the \code{Microsoft.NETCore.App} shared runtime.
* \code{aspnetcore} - the \code{Microsoft.AspNetCore.App} shared runtime.}
}
\description{
Install the dotnet core runtime on your machine.
For more informations on this method see \href{https://docs.microsoft.com/en-us/dotnet/core/tools/dotnet-install-script}{dotnet-install-script}
}
\examples{
\dontrun{
library(sharper)

install_dotnet_core()
install_dotnet_core(installDir = "./", runtime = "aspnetcore")
}
}
