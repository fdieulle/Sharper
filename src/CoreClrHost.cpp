#include "CoreClrHost.h"

releaseObject_ptr CoreClrHost::releaseObjectFunc;

CoreClrHost::CoreClrHost()
{
}


CoreClrHost::~CoreClrHost()
{
}


void CoreClrHost::start(const char* app_base_dir, const char* package_bin_folder, const char* dotnet_install_path)
{
	Rprintf("app_base_dir: %s\n", app_base_dir);
	Rprintf("package_bin_folder: %s\n", package_bin_folder);
	Rprintf("dotnet_install_path: %s\n", dotnet_install_path);

	if (_coreClr != NULL || _hostHandle != NULL)
		shutdown();

	if (app_base_dir == NULL) app_base_dir = ".";
	std::string app_base_dir_exp;
	path_expand(app_base_dir, app_base_dir_exp);

	std::string tpa_list;
	std::string core_clr_path;
	if (!get_core_clr_with_tpa_list(app_base_dir_exp.c_str(), package_bin_folder, dotnet_install_path, core_clr_path, tpa_list))
	{
		Rf_warning("Please install a dotnet core runtime version first.\nSee install_dotnet_core function.\n");
		return;
	}
	
	Rprintf("Load %s from: %s\n", CORECLR_FILE_NAME, core_clr_path.c_str());

	// 1. Load CoreCLR (coreclr.dll/libcoreclr.so)
#if WINDOWS
	_coreClr = LoadLibraryExA(core_clr_path.c_str(), NULL, 0);
#elif LINUX
	_coreClr = dlopen(core_clr_path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

	if (_coreClr == NULL)
	{
		Rf_error("Failed to load CoreCLR from %s\n", core_clr_path.c_str());
		return;
	}

	// 2. Get CoreCLR hosting functions
#if WINDOWS
	_initializeCoreClr = (coreclr_initialize_ptr)GetProcAddress(_coreClr, "coreclr_initialize");
	_createManagedDelegate = (coreclr_create_delegate_ptr)GetProcAddress(_coreClr, "coreclr_create_delegate");
	_shutdownCoreClr = (coreclr_shutdown_ptr)GetProcAddress(_coreClr, "coreclr_shutdown");
#elif LINUX
	_initializeCoreClr = (coreclr_initialize_ptr)dlsym(_coreClr, "coreclr_initialize");
	_createManagedDelegate = (coreclr_create_delegate_ptr)dlsym(_coreClr, "coreclr_create_delegate");
	_shutdownCoreClr = (coreclr_shutdown_ptr)dlsym(_coreClr, "coreclr_shutdown");
#endif

	if (_initializeCoreClr == NULL)
	{
		Rf_error("coreclr_initialize not found");
		return;
	}

	if (_createManagedDelegate == NULL)
	{
		Rf_error("coreclr_create_delegate not found");
		return;
	}

	if (_shutdownCoreClr == NULL)
	{
		Rf_error("coreclr_shutdown not found");
		return;
	}

	// 3. Construct properties used when starting the runtime
	// Define CoreCLR properties
	// Other properties related to assembly loading are common here
	const char* propertyKeys[] = {
		"TRUSTED_PLATFORM_ASSEMBLIES"      // Trusted assemblies
	};

	const char* propertyValues[] = {
		tpa_list.c_str()
	};

	// 4. Start the CoreCLR runtime and create the default (and only) AppDomain
	int hr = _initializeCoreClr(
		app_base_dir_exp.c_str(),        // App base path
		"CoreClrHost",       // AppDomain friendly name
		sizeof(propertyKeys) / sizeof(char*),   // Property count
		propertyKeys,       // Property names
		propertyValues,     // Property values
		&_hostHandle,        // Host handle
		&_domainId); // AppDomain ID

	if (hr >= 0)
		Rprintf("CoreCLR started\n");
	else
	{
		Rf_error("coreclr_initialize failed - status: 0x%08x\n", hr);
		return;
	}

	// 5. Create delegates to managed code to be able to invoke them
	createManagedDelegate("GetLastError", (void**)&_getLastErrorFunc);
	createManagedDelegate("LoadAssembly", (void**)&_loadAssemblyFunc);
	createManagedDelegate("CallStaticMethod", (void**)&_callStaticMethodFunc);
	createManagedDelegate("GetStaticProperty", (void**)&_getStaticPropertyFunc);
	createManagedDelegate("SetStaticProperty", (void**)&_setStaticPropertyFunc);
	createManagedDelegate("CreateObject", (void**)&_createObjectFunc);
	createManagedDelegate("CallMethod", (void**)&_callFunc);
	createManagedDelegate("ReleaseObject", (void**)&(CoreClrHost::releaseObjectFunc));
	createManagedDelegate("GetProperty", (void**)&_getFunc);
	createManagedDelegate("SetProperty", (void**)&_setFunc);
}

void CoreClrHost::shutdown()
{
	int hr = _shutdownCoreClr(_hostHandle, _domainId);

	if (hr >= 0)
	{
		Rprintf("CoreCLR successfully shutdown\n");
		_hostHandle = NULL;
		_domainId = 0;
	}
	else Rf_error("coreclr_shutdown failed - status: 0x%08x\n", hr);
	
	// Unload CoreCLR
#if WINDOWS
	if (!FreeLibrary(_coreClr))
		Rf_error("Failed to free coreclr.dll\n");
#elif LINUX
	if (dlclose(_coreClr))
		Rf_error("Failed to free libcoreclr.so\n");
#endif

	_coreClr = NULL;
}

const char* CoreClrHost::getLastError() 
{
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		return "CoreCLR isn't started.";
	}

	return _getLastErrorFunc();
}

bool CoreClrHost::loadAssembly(const char * filePath)
{
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _loadAssemblyFunc(filePath);
}

bool CoreClrHost::callStaticMethod(const char* typeName, const char* methodName, int64_t* args, int32_t argsSize, int64_t** results, int32_t* resultsSize)
{
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _callStaticMethodFunc(typeName, methodName, args, argsSize, results, resultsSize);
}

bool CoreClrHost::getStaticProperty(const char* typeName, const char* propertyName, int64_t* value)
{
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _getStaticPropertyFunc(typeName, propertyName, value);
}

bool CoreClrHost::setStaticProperty(const char* typeName, const char* propertyName, int64_t value)
{
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _setStaticPropertyFunc(typeName, propertyName, value);
}

bool CoreClrHost::createObject(const char* typeName, int64_t* args, int32_t argsSize, int64_t* value)
{
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _createObjectFunc(typeName, args, argsSize, value);
}

void CoreClrHost::registerFinalizer(SEXP sexp)
{
	R_RegisterCFinalizerEx(sexp, [](SEXP p) { CoreClrHost::releaseObjectFunc((int64_t)p); }, (Rboolean)1);
}

bool CoreClrHost::callMethod(int64_t objectPtr, const char* methodName, int64_t* args, int32_t argsSize, int64_t** results, int32_t* resultsSize) {
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _callFunc(objectPtr, methodName, args, argsSize, results, resultsSize);
}

bool CoreClrHost::getProperty(int64_t objectPtr, const char* propertyName, int64_t* value) {
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _getFunc(objectPtr, propertyName, value);
}

bool CoreClrHost::setProperty(int64_t objectPtr, const char* propertyName, int64_t value) {
	if (_coreClr == NULL && _hostHandle == NULL)
	{
		Rf_error("CoreCLR isn't started.");
		return true;
	}

	return _setFunc(objectPtr, propertyName, value);
}

/*static*/ void CoreClrHost::build_tpa_list(const char* directory, const char* extension, std::string& tpaList)
{
#if WINDOWS
	// Win32 directory search for .dll files

	// This will add all files with a .dll extension to the TPA list. 
	// This will include unmanaged assemblies (coreclr.dll, for example) that don't
	// belong on the TPA list. In a real host, only managed assemblies that the host
	// expects to load should be included. Having extra unmanaged assemblies doesn't
	// cause anything to fail, though, so this function just enumerates all dll's in
	// order to keep this sample concise.
	std::string searchPath(directory);
	searchPath.append(FS_SEPERATOR);
	searchPath.append("*");
	searchPath.append(extension);

	WIN32_FIND_DATAA findData;
	HANDLE fileHandle = FindFirstFileA(searchPath.c_str(), &findData);
	
	if (fileHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Append the assembly to the list
			tpaList.append(directory);
			tpaList.append(FS_SEPERATOR);
			tpaList.append(findData.cFileName);
			tpaList.append(PATH_DELIMITER);

			// Note that the CLR does not guarantee which assembly will be loaded if an assembly
			// is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
			// extensions. Therefore, a real host should probably add items to the list in priority order and only
			// add a file if it's not already present on the list.
			//
			// For this simple sample, though, and because we're only loading TPA assemblies from a single path,
			// and have no native images, we can ignore that complication.
		} while (FindNextFileA(fileHandle, &findData));
		FindClose(fileHandle);
	}
#elif LINUX
	// POSIX directory search for .dll files
	DIR* dir = opendir(directory);
	struct dirent* entry;
	int extLength = strlen(extension);

	while ((entry = readdir(dir)) != NULL)
	{
		// This simple sample doesn't check for symlinks
		std::string filename(entry->d_name);

		// Check if the file has the right extension
		int extPos = filename.size() - extLength;
		if (extPos <= 0 || filename.compare(extPos, extLength, extension) != 0)
		{
			continue;
		}

		// Append the assembly to the list
		tpaList.append(directory);
		tpaList.append(FS_SEPERATOR);
		tpaList.append(filename);
		tpaList.append(PATH_DELIMITER);

		// Note that the CLR does not guarantee which assembly will be loaded if an assembly
		// is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
		// extensions. Therefore, a real host should probably add items to the list in priority order and only
		// add a file if it's not already present on the list.
		//
		// For this simple sample, though, and because we're only loading TPA assemblies from a single path,
		// and have no native images, we can ignore that complication.
	}
#endif
}

void CoreClrHost::createManagedDelegate(const char* entryPointMethodName, void** delegate)
{
	int hr = _createManagedDelegate(
		_hostHandle,
		_domainId,
		"Sharper",
		"Sharper.ClrProxy",
		entryPointMethodName,
		delegate);

	if (hr < 0)
		Rf_error("coreclr_create_delegate for method %s failed - status: %d\n", entryPointMethodName, hr);
}

struct version_t {
	version_t() : version_t(-1, -1, -1, -1) { }
	version_t(int major, int minor, int build, int revision)
		: m_major(major), m_minor(minor), m_build(build), m_revision(revision) { }

	int get_major() const { return m_major; }
	int get_minor() const { return m_minor; }
	int get_build() const { return m_build; }
	int get_revision() const { return m_revision; }

	void set_major(int m) { m_major = m; }
	void set_minor(int m) { m_minor = m; }
	void set_build(int m) { m_build = m; }
	void set_revision(int m) { m_revision = m; }

	bool operator ==(const version_t& b) const { return compare(*this, b) == 0; }
	bool operator !=(const version_t& b) const { return !operator ==(b); }
	bool operator <(const version_t& b) const { return compare(*this, b) < 0; }
	bool operator >(const version_t& b) const { return compare(*this, b) > 0; }
	bool operator <=(const version_t& b) const { return compare(*this, b) <= 0; }
	bool operator >=(const version_t& b) const { return compare(*this, b) >= 0; }

	std::string as_str() const {
		std::string version;
		if (m_major >= 0)
		{
			version.append(std::to_string(m_major));
			if (m_minor >= 0)
			{
				version.append(std::to_string(m_minor));
				if (m_build >= 0)
				{
					version.append(std::to_string(m_build));
					if (m_revision >= 0)
						version.append(std::to_string(m_revision));
				}
			}
		}

		return version;
	}

	static bool parse(const std::string& version, version_t* version_out) {
		
		if (version.empty()) return false;

		size_t start = 0;
		int major = parse_next(version, &start);
		int minor = parse_next(version, &start);
		int build = parse_next(version, &start);
		int revision = parse_next(version, &start);
		
		*version_out = version_t(major, minor, build, revision);

		return true;
	}

private:
	int m_major;
	int m_minor;
	int m_build;
	int m_revision;

	static int compare(const version_t&a, const version_t& b) {
		if (a.m_major != b.m_major)
			return (a.m_major > b.m_major) ? 1 : -1;

		if (a.m_minor != b.m_minor)
			return (a.m_minor > b.m_minor) ? 1 : -1;

		if (a.m_build != b.m_build)
			return (a.m_build > b.m_build) ? 1 : -1;

		if (a.m_revision != b.m_revision)
			return (a.m_revision > b.m_revision) ? 1 : -1;

		return 0;
	}

	static int parse_next(const std::string& version, size_t* offset) {
		if (version.size() - *offset <= 0) return -1;

		size_t start = *offset;
		size_t length = version.find('.', start);
		*offset = start + length;
		if (length == std::string::npos)
			return std::stoi(version.substr(start));

		*offset = (*offset) + 1;
		return std::stoi(version.substr(start, length));
	}
};

//bool CoreClrHost::get_core_clr(const char* app_base_dir, const char* dotnet_install_path, std::string& core_clr, bool& is_self_contained) {
//	
//	std::string app_base_dir_exp;
//
//	if (!is_directory(app_base_dir) && file_exists(app_base_dir))
//		path_get_parent(app_base_dir, app_base_dir_exp);
//	else app_base_dir_exp = app_base_dir;
//
//	// First try if the app base dir is self contained
//	if (is_directory(app_base_dir_exp))
//	{
//
//	}
//}
//
//bool CoreClrHost::is_self_contained(const char* app_base_dir) {
//	
//
//	std::string core_clr;
//	path_combine(app_base_dir_exp, CORECLR_FILE_NAME);
//}

/*static*/ bool CoreClrHost::get_core_clr_with_tpa_list(
	const char* app_base_dir, 
	const char* package_bin_folder, 
	const char* dotnet_install_path, 
	std::string& core_clr,
	std::string& tpa_list) {
	
	if (is_directory(package_bin_folder))
		CoreClrHost::build_tpa_list(package_bin_folder, ".dll", tpa_list);

	std::string app_base_dir_exp;
	path_expand(
		!is_directory(app_base_dir) && file_exists(app_base_dir) // If the given path is a file we get the parent folder
			? path_get_parent(app_base_dir)
			: app_base_dir,
		app_base_dir_exp);
	std::string dotnet_install_path_exp;
	path_expand(dotnet_install_path, dotnet_install_path_exp);

	if (is_directory(app_base_dir_exp.c_str()))
	{
		CoreClrHost::build_tpa_list(app_base_dir_exp.c_str(), ".dll", tpa_list);
		CoreClrHost::build_tpa_list(app_base_dir_exp.c_str(), ".exe", tpa_list);
		
		// Check if the app_base_dir is self contained
		std::string core_clr;
		path_combine(app_base_dir_exp, CORECLR_FILE_NAME, core_clr);
		if (file_exists(core_clr.c_str()))
		{
			return true;
		}
	}
	
	// Load the dotnet core dlls.
	if (is_directory(dotnet_install_path_exp.c_str()))
	{
		std::string core_clr;
		path_combine(dotnet_install_path_exp, CORECLR_FILE_NAME, core_clr);
		if (file_exists(core_clr.c_str()))
		{
			CoreClrHost::build_tpa_list(dotnet_install_path_exp.c_str(), ".dll", tpa_list);
			return true;
		}
	}

	return false;
}

