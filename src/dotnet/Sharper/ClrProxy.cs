﻿using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using Sharper.Converters;
using Sharper.Converters.RDotNet;
using Sharper.Loggers;

namespace Sharper
{
    public static class ClrProxy
    {
        private static readonly ILogger logger = new FileLogger(Assembly.GetExecutingAssembly().Location);

        static ClrProxy()
        {
            AppDomain.CurrentDomain.UnhandledException += OnException;
        }

        private static void OnException(object sender, UnhandledExceptionEventArgs e)
        {
            logger.Error("Unhandled exception", e.ExceptionObject as Exception);
        }

        #region Mange Data converter

        public static IDataConverter DataConverter { get; set; } = new RDotNetConverter(logger);

        #endregion

        public static void LoadAssembly([MarshalAs(UnmanagedType.LPStr)] string pathOrAssemblyName)
        {
            logger.InfoFormat("[LoadAssembly] Path or AssemblyName: {0}", pathOrAssemblyName);

            if (string.IsNullOrEmpty(pathOrAssemblyName))
                return;

            try
            {
                var filePath = pathOrAssemblyName.Replace("/", "\\");
                if (File.Exists(filePath))
                {
                    var assemblyName = new FileInfo(filePath).Name;
                    foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
                    {
                        if (string.Equals(assembly.ManifestModule.Name, assemblyName))
                            return;
                    }

                    Assembly.LoadFrom(filePath);
                    return;
                }

                if (pathOrAssemblyName.IsFullyQualifiedAssemblyName())
                {
                    Assembly.Load(pathOrAssemblyName);
                    return;
                }

                throw new FileLoadException($"Unable to load assembly: {pathOrAssemblyName}");
            }
            catch (Exception e)
            {
                logger.Error("[LoadAssembly]", e);
                throw;
            }
        }

        public static void CallStaticMethod(
            [MarshalAs(UnmanagedType.LPStr)] string typeName,
            [MarshalAs(UnmanagedType.LPStr)] string methodName,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 3)] long[] argumentsPtr,
            int argumentsSize,
            [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 5)] out long[] results,
            [Out] out int resultsSize)
        {
            const BindingFlags flags = BindingFlags.Public | BindingFlags.Static;

            logger.DebugFormat("[CallStaticMethod] TypeName: {0}, MethodName: {1}, NbArguments: {2}", typeName, methodName, argumentsSize);

            try
            {
                if (!typeName.TryGetType(out var type, out var errorMsg))
                    throw new TypeAccessException(errorMsg);

                var converters = new IConverter[argumentsSize];
                for (var i = 0; i < argumentsSize; i++)
                    converters[i] = DataConverter.GetConverter(argumentsPtr[i]);

                if (!type.TryGetMethod(methodName, flags, converters, out var method))
                    throw new MissingMethodException($"Method not found, Type: {typeName}, Method: {methodName}");

                InternalCallMethod(method, null, converters, out results, out resultsSize);
            }
            catch (Exception e)
            {
                logger.Error("[CallStaticMethod]", e);
                throw;
            }
        }

        [return: MarshalAs(UnmanagedType.U8)]
        public static long GetStaticProperty(
            [MarshalAs(UnmanagedType.LPStr)] string typeName,
            [MarshalAs(UnmanagedType.LPStr)] string propertyName)
        {
            logger.DebugFormat("[GetStaticProperty] TypeName: {0}, PropertyName: {1}", typeName, propertyName);

            try
            {
                if (!typeName.TryGetType(out var type, out var errorMsg))
                    throw new TypeAccessException(errorMsg);

                var property = type.GetProperty(propertyName, BindingFlags.Public | BindingFlags.Static);
                if (property == null)
                    throw new MissingMemberException($"Static property {propertyName} not found for Type: {type.FullName}");
                if (!property.CanRead)
                    throw new InvalidOperationException($"Static property {propertyName} can't be get for Type: {type.FullName}");

                var result = property.GetGetMethod().Call(null, new IConverter[0])[0];
                return DataConverter.ConvertBack(property.PropertyType, result);
            }
            catch (Exception e)
            {
                logger.Error("[GetStaticProperty]", e);
                throw;
            }
        }

        public static void SetStaticProperty(
            [MarshalAs(UnmanagedType.LPStr)] string typeName,
            [MarshalAs(UnmanagedType.LPStr)] string propertyName,
            [MarshalAs(UnmanagedType.U8)] long argumentPtr)
        {
            logger.DebugFormat("[SetStaticProperty] TypeName: {0}, PropertyName: {1}", typeName, propertyName);

            try
            {
                if (!typeName.TryGetType(out var type, out var errorMsg))
                    throw new TypeAccessException(errorMsg);

                var property = type.GetProperty(propertyName, BindingFlags.Public | BindingFlags.Static);
                if (property == null)
                    throw new MissingMemberException($"Static property {propertyName} not found for Type: {type.FullName}");
                if (!property.CanWrite)
                    throw new InvalidOperationException($"Static property {propertyName} can't be set for Type: {type.FullName}");

                var converters = new[] { DataConverter.GetConverter(argumentPtr) };

                property.GetSetMethod().Call(null, converters);
            }
            catch (Exception e)
            {
                logger.Error("[SetStaticProperty]", e);
                throw;
            }
        }

        [return: MarshalAs(UnmanagedType.U8)]
        public static long CreateObject(
            [MarshalAs(UnmanagedType.LPStr)] string typeName,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] long[] argumentsPtr,
            int argumentsSize)
        {
            logger.DebugFormat("[CreateInstance] TypeName: {0}", typeName);

            try
            {
                if (!typeName.TryGetType(out var type, out var errorMsg))
                    throw new TypeAccessException(errorMsg);

                var converters = new IConverter[argumentsSize];
                for (var i = 0; i < argumentsSize; i++)
                    converters[i] = DataConverter.GetConverter(argumentsPtr[i]);

                if (!type.TryGetConstructor(converters, out var ctor))
                    throw new MissingMemberException($"Constructor not found for Type: {typeName}");

                var result = ctor.Call(converters);
                return DataConverter.ConvertBack(type, result);
            }
            catch (Exception e)
            {
                logger.Error("[CreateInstance]", e);
                throw;
            }
        }

        public static void ReleaseObject([MarshalAs(UnmanagedType.U8)] long objectPtr)
        {
            logger.Debug("[ReleaseObject]");
            try
            {
                DataConverter.Release(objectPtr);
            }
            catch (Exception e)
            {
                logger.Error("[ReleaseObject]", e);
                throw;
            }
        }

        public static void CallMethod(
            [MarshalAs(UnmanagedType.U8)]  long objectPtr,
            [MarshalAs(UnmanagedType.LPStr)] string methodName,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 3)] long[] argumentsPtr,
            int argumentsSize,
            [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 5)] out long[] results,
            [Out] out int resultsSize)
        {
            const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance;

            logger.DebugFormat("[CallMethod] Instance: {0}, MethodName: {1}", objectPtr, methodName);

            try
            {
                var instance = DataConverter.GetConverter(objectPtr)?.Convert(typeof(object));
                if (instance == null)
                    throw new ArgumentNullException(nameof(objectPtr));

                var type = instance.GetType();

                var converters = new IConverter[argumentsSize];
                for (var i = 0; i < argumentsSize; i++)
                    converters[i] = DataConverter.GetConverter(argumentsPtr[i]);

                if (!type.TryGetMethod(methodName, flags, converters, out var method))
                    throw new MissingMethodException($"Method not found for Type: {type}, Method: {methodName}");

                InternalCallMethod(method, instance, converters, out results, out resultsSize);
            }
            catch (Exception e)
            {
                logger.Error("[CallMethod]", e);
                //LastException = Format(e);
                throw;
            }
        }

        [return: MarshalAs(UnmanagedType.U8)]
        public static long GetProperty(
            [MarshalAs(UnmanagedType.U8)]  long objectPtr,
            [MarshalAs(UnmanagedType.LPStr)] string propertyName)
        {
            logger.DebugFormat("[GetProperty] Instance: {0}, PropertyName: {1}", objectPtr, propertyName);

            try
            {
                var instance = DataConverter.GetConverter(objectPtr)?.Convert(typeof(object));
                if (instance == null)
                    throw new ArgumentNullException(nameof(objectPtr));

                var type = instance.GetType();

                var property = type.GetProperty(propertyName);
                if (property == null)
                    throw new MissingMemberException($"Property {propertyName} not found for Type: {type.FullName}");

                if (!property.CanRead)
                    throw new InvalidOperationException($"Property {propertyName} can't be get for Type: {type.FullName}");

                var result = property.GetGetMethod().Call(instance, new IConverter[0])[0];
                return DataConverter.ConvertBack(property.PropertyType, result);
            }
            catch (Exception e)
            {
                logger.Error("[GetProperty]", e);
                //LastException = Format(e);
                throw;
            }
        }

        public static void SetProperty(
            [MarshalAs(UnmanagedType.U8)] long objectPtr, 
            [MarshalAs(UnmanagedType.LPStr)] string propertyName,
            [MarshalAs(UnmanagedType.U8)] long argumentPtr)
        {
            logger.DebugFormat("[SetProperty] Instance: {0}, PropertyName: {1}", objectPtr, propertyName);

            try
            {
                var instance = DataConverter.GetConverter(objectPtr)?.Convert(typeof(object));
                if (instance == null)
                    throw new ArgumentNullException(nameof(objectPtr));

                var type = instance.GetType();

                var property = type.GetProperty(propertyName);
                if (property == null)
                    throw new MissingMemberException($"Property {propertyName} not found for Type: {type.FullName}");

                if (!property.CanWrite)
                    throw new InvalidOperationException($"Property {propertyName} can't be set for Type: {type.FullName}");

                var converters = new[] { DataConverter.GetConverter(argumentPtr) };

                property.GetSetMethod().Call(instance, converters);
            }
            catch (Exception e)
            {
                logger.Error("[SetProperty]", e);
                //LastException = Format(e);
                throw;
            }
        }

        private static void InternalCallMethod(MethodInfo method, object instance, IConverter[] converters, out long[] results, out int resultsSize)
        {
            var objects = method.Call(instance, converters);
            resultsSize = objects.Length;
            results = new long[resultsSize];

            results[0] = DataConverter.ConvertBack(method.ReturnType, objects[0]);
            if (resultsSize > 1)
            {
                var parameters = method.GetParameters();
                for (var i = 1; i < results.Length; i++)
                    results[i] = DataConverter.ConvertBack(parameters[i - 1].ParameterType.Extract(), objects[i]);
            }
        }
    }
}
