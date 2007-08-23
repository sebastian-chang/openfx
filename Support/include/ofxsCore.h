#ifndef _ofxsCore_H_
#define _ofxsCore_H_
/*
  OFX Support Library, a library that skins the OFX plug-in API with C++ classes.
  Copyright (C) 2004-2005 The Foundry Visionmongers Ltd
  Author Bruno Nicoletti bruno@thefoundry.co.uk

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name The Foundry Visionmongers Ltd, nor the names of its 
      contributors may be used to endorse or promote products derived from this
      software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The Foundry Visionmongers Ltd
1 Wardour St
London W1D 6PA
England



*/


/** @mainpage OFX Support Library
 
@section mainpageIntro Introduction

This support library skins the raw OFX C API with a set of C++ classes and functions that makes it easier to understand and write plug-ins to the API. Look at the examples to see how it is done.

<HR>

@section fifteenLineGuide Fifteen Line Plugin Writing Guide

- work from the examples
- you need to write the following functions....
  - void OFX::Plugin::getPluginID(OFX::PluginID &id)
    - gives the unique name and version numbers of the plug-in
  - void OFX::Plugin::loadAction(void)
    - called after the plug-in is first loaded, and before any instance has been made,
  - void OFX::Plugin::unloadAction(void)
    - called before the plug-in is unloaded, and all instances have been destroyed,
  - void OFX::Plugin::describe(OFX::ImageEffectDescriptor &desc) 
    - called to describe the plugin to the host
  - void OFX::Plugin::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) 
    - called to describe the plugin to the host for a context reported in OFX::Plugin::describe
  -  OFX::ImageEffect * OFX::Plugin::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context)
    - called when a new instance of a plug-in needs to be created. You need to derive a class from ImageEffect, new it and return it.

The OFX::ImageEffect class has a set of members you can override to do various things, like rendering an effect. Again, look at the examples.

<HR>

@section license Copyright and License

     The library is copyright 2004-2005, The Foundry Visionmongers Ltd, and was
     written by Bruno Nicoletti (bruno@thefoundry.co.uk).

     It has been released under the GNU Lesser General Public License, see the 
     top of any source file for details.

*/

/** @file This file contains core code that wraps OFX 'objects' with C++ classes.

This file only holds code that is visible to a plugin implementation, and so hides much
of the direct OFX objects and any library side only functions.
 */

#ifdef _MSC_VER
#pragma warning( disable : 4290 )
#endif

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxInteract.h"
#include "ofxKeySyms.h"
#include "ofxMemory.h"
#include "ofxMessage.h"
#include "ofxMultiThread.h"
#include "ofxParam.h"
#include "ofxProperty.h"

#include <assert.h>
#include <vector>
#include <string>
#include <map>
#include <exception>
#include <stdexcept>

#ifdef OFX_CLIENT_EXCEPTION_HEADER
#include OFX_CLIENT_EXCEPTION_HEADER
#endif

/** @brief Defines an 8 bit per component RGB pixel 

Should migrate this to the ofxCore.h in a v1.1
*/
struct OfxRGBColourB {
    unsigned char r, g, b;
};

/** @brief Defines a 16 bit per component RGB pixel

Should migrate this to the ofxCore.h in a v1.1
*/
struct OfxRGBColourS {
    unsigned short r, g, b;
};

/** @brief Defines a floating point component RGB pixel

Should migrate this to the ofxCore.h in a v1.1
*/
struct OfxRGBColourF {
    float r, g, b;
};

/** @brief Defines a double precision floating point component RGB pixel

Should migrate this to the ofxCore.h in a v1.1
*/
struct OfxRGBColourD {
    double r, g, b;
};

/** @brief Defines an integer 3D point

Should migrate this to the ofxCore.h in a v1.1
*/
struct Ofx3DPointI {
    int x, y, z;
};

/** @brief Defines a double precision 3D point

Should migrate this to the ofxCore.h in a v1.1
*/
struct Ofx3DPointD {
    double x, y, z;
};

/** @brief Nasty macro used to define empty protected copy ctors and assign ops */
#define mDeclareProtectedAssignAndCC(CLASS) \
  CLASS &operator=(const CLASS &v1) {assert(false); return *this;}	\
  CLASS(const CLASS &v) {assert(false); } 

/** @brief The core 'OFX Support' namespace, used by plugin implementations. All code for these are defined in the common support libraries.
 */
namespace OFX {
  /** forward class declarations */
  class PropertySet;

  /** @brief Enumerates the different types a property can be */
  enum PropertyTypeEnum {
    ePointer,
    eInt,
    eString,
    eDouble};

  /** @brief Enumerates the reasons a plug-in instance may have had one of its values changed */
  enum InstanceChangeReason {
    eChangeUserEdit,    /**< @brief A user actively editted something in the plugin, eg: changed the value of an integer param on an interface */
    eChangePluginEdit,  /**< @brief The plugin's own code changed something in the instance, eg: a callback on on param settting the value of another */
    eChangeTime         /**< @brief The current value of a parameter has changed because the param animates and the current time has changed */
  };

  /** @brief maps a status to a string for debugging purposes, note a c-str for printf */
  char * mapStatusToString(OfxStatus stat);

  /** @brief namespace for OFX support lib exceptions, all derive from std::exception, calling it */
  namespace Exception {

    /** @brief thrown when a suite returns a dud status code 
     */
    class Suite : public std::exception {
    protected :
      OfxStatus _status;
    public :
      Suite(OfxStatus s) : _status(s) {}
      OfxStatus status(void) {return _status;}
      operator OfxStatus() {return _status;}

      /** @brief reimplemented from std::exception */
      virtual const char * what () const throw () {return mapStatusToString(_status);}

    };

    /** @brief Exception indicating that a host doesn't know about a property that is should do */
    class PropertyUnknownToHost : public std::exception {
    protected :
      std::string _what;
    public :
      PropertyUnknownToHost(const char *what) : _what(what) {}
      virtual ~PropertyUnknownToHost() throw() {}

      /** @brief reimplemented from std::exception */
      virtual const char * what () const throw ()
      {
        return _what.c_str();
      }
    };

    /** @brief exception indicating that the host thinks a property has an illegal value */
    class PropertyValueIllegalToHost : public std::exception {
    protected :
      std::string _what;
    public :
      PropertyValueIllegalToHost(const char *what) : _what(what) {}
      virtual ~PropertyValueIllegalToHost() throw() {}

      /** @brief reimplemented from std::exception */
      virtual const char * what () const throw ()
      {
        return _what.c_str();
      }
    };

    /** @brief exception indicating a request for a named thing exists (eg: a param), but is of the wrong type, should never make it back to the main entry
        indicates a logical error in the code. Asserts are raised in debug code in these situations.
    */
    class TypeRequest : public std::exception {
    protected :
      std::string _what;
    public :
      TypeRequest(const char *what) : _what(what) {}
      virtual ~TypeRequest() throw() {}

      /** @brief reimplemented from std::exception */
      virtual const char * what () const throw ()
      {
        return _what.c_str();
      }
    };

    ////////////////////////////////////////////////////////////////////////////////
    // These exceptions are to be thrown by the plugin if it hits a problem, the
    // code managing the main entry will trap the exception and return a suitable 
    // status code to the host.

    /** @brief exception indicating a required host feature is missing */
    class HostInadequate : public std::exception {
    protected :
      std::string _what;
    public :
      HostInadequate(char *what) : _what(what) {}
      virtual ~HostInadequate() throw() {}

      /** @brief reimplemented from std::exception */
      virtual const char * what () const throw ()
      {
        return _what.c_str();
      }
    };

  }; // end of Exception namespace

  /** @brief Throws an @ref OFX::Exception::Suite depending on the status flag passed in */
  void 
  throwSuiteStatusException(OfxStatus stat) 
    throw(OFX::Exception::Suite, std::bad_alloc);

  void 
  throwHostMissingSuiteException(std::string name) 
    throw(OFX::Exception::Suite);

  /** @brief This struct is used to return an identifier for the plugin by the function @ref OFX:Plugin::getPlugin. 
      The members correspond to those in the OfxPlugin struct defined in ofxCore.h.
  */
  struct PluginID {
    std::string  pluginIdentifier;
    unsigned int pluginVersionMajor;
    unsigned int pluginVersionMinor;
  };

  /** @brief This class wraps up an OFX property set */
  class PropertySet {
  protected :
    /** @brief The raw property handle */
    OfxPropertySetHandle _propHandle;

    /** @brief Class static, whether we are logging each property action */
    static int _gPropLogging;

    /** @brief Do not throw an exception if a host returns 'unsupported' when setting a property */
    static bool _gThrowOnUnsupported;

  public :
    /** @brief turns on logging of property access functions */
    static void propEnableLogging(void)  {++_gPropLogging;}
    
    /** @brief turns off logging of property access functions */
    static void propDisableLogging(void) {--_gPropLogging;}

    /** @brief Do we throw an exception if a host returns 'unsupported' when setting a property. Default is true */
    static void setThrowOnUnsupportedProperties(bool v) {_gThrowOnUnsupported = v;}

    /** @brief Do we throw an exception if a host returns 'unsupported' when setting a property. Default is true */
    static bool getThrowOnUnsupportedProperties(void) {return _gThrowOnUnsupported;}

    /** @brief construct a property set */
    PropertySet(OfxPropertySetHandle h = 0) : _propHandle(h) {}
    virtual ~PropertySet();

    /** @brief set the handle to use for this set */
    void propSetHandle(OfxPropertySetHandle h) { _propHandle = h;}

    /** @brief return the handle for this property set */
    OfxPropertySetHandle propSetHandle(void) {return _propHandle;}

    int  propGetDimension(const char* property, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                               OFX::Exception::PropertyUnknownToHost, 
                                                                                               OFX::Exception::PropertyValueIllegalToHost, 
                                                                                               OFX::Exception::Suite);
    void propReset(const char* property) throw(std::bad_alloc, 
                                                      OFX::Exception::PropertyUnknownToHost, 
                                                      OFX::Exception::PropertyValueIllegalToHost,
                                                      OFX::Exception::Suite);

    // set single values
    void propSetPointer(const char* property, void *value, int idx, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                             OFX::Exception::PropertyUnknownToHost, 
                                                                                                             OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                             OFX::Exception::Suite);
    void propSetString(const char* property, const std::string &value, int idx, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                                         OFX::Exception::PropertyUnknownToHost, 
                                                                                                                         OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                                         OFX::Exception::Suite);
    void propSetDouble(const char* property, double value, int idx, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                             OFX::Exception::PropertyUnknownToHost, 
                                                                                                             OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                             OFX::Exception::Suite);
    void propSetInt(const char* property, int value, int idx, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                       OFX::Exception::PropertyUnknownToHost, 
                                                                                                       OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                       OFX::Exception::Suite);

    void propSetPointer(const char* property, void *value, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                    OFX::Exception::PropertyUnknownToHost, 
                                                                                                    OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                    OFX::Exception::Suite)
    {propSetPointer(property, value, 0, throwOnFailure);}

    void propSetString(const char* property, const std::string &value, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                                OFX::Exception::PropertyUnknownToHost, 
                                                                                                                OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                                OFX::Exception::Suite)
    {propSetString(property, value, 0, throwOnFailure);}

    void propSetDouble(const char* property, double value, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                                    OFX::Exception::PropertyUnknownToHost, 
                                                                                                    OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                    OFX::Exception::Suite)
    {propSetDouble(property, value, 0, throwOnFailure);}
    
    void propSetInt(const char* property, int value, bool throwOnFailure = true) throw(std::bad_alloc, 
                                                                                              OFX::Exception::PropertyUnknownToHost, 
                                                                                              OFX::Exception::PropertyValueIllegalToHost, 
                                                                                              OFX::Exception::Suite)
    {propSetInt(property, value, 0, throwOnFailure);}
    
    
    /// get a pointer property
    void       *propGetPointer(const char* property, int idx, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                             OFX::Exception::PropertyUnknownToHost, 
                                                                                                             OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                             OFX::Exception::Suite);

    /// get a string property
    std::string propGetString(const char* property, int idx, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                            OFX::Exception::PropertyUnknownToHost, 
                                                                                                            OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                            OFX::Exception::Suite);
    /// get a double property
    double      propGetDouble(const char* property, int idx, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                            OFX::Exception::PropertyUnknownToHost, 
                                                                                                            OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                            OFX::Exception::Suite);

    /// get an int property
    int         propGetInt(const char* property, int idx, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                         OFX::Exception::PropertyUnknownToHost, 
                                                                                                         OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                         OFX::Exception::Suite);
    
    /// get a string property with index 0
    void       *propGetPointer(const char* property, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                    OFX::Exception::PropertyUnknownToHost, 
                                                                                                    OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                    OFX::Exception::Suite)
    {return propGetPointer(property, 0, throwOnFailure); }
    
    /// get a string property with index 0
    std::string propGetString(const char* property, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                   OFX::Exception::PropertyUnknownToHost, 
                                                                                                   OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                   OFX::Exception::Suite)
    {return propGetString(property, 0, throwOnFailure); }
    
    /// get a double property with index 0
    double      propGetDouble(const char* property, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                   OFX::Exception::PropertyUnknownToHost, 
                                                                                                   OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                   OFX::Exception::Suite)
    {return propGetDouble(property, 0, throwOnFailure); }
    
    /// get an int property with index 0
    int         propGetInt(const char* property, bool throwOnFailure = true) const throw(std::bad_alloc, 
                                                                                                OFX::Exception::PropertyUnknownToHost, 
                                                                                                OFX::Exception::PropertyValueIllegalToHost, 
                                                                                                OFX::Exception::Suite)
    {return propGetInt(property, 0, throwOnFailure); }
    

  };

  ////////////////////////////////////////////////////////////////////////////////

  // forward decl of the image effect
  class ImageEffect;

  /** @brief namespace for memory allocation that is done via wrapping the ofx memory suite */
  namespace Memory {
    /** @brief allocate n bytes, returns a pointer to it */
    void *alloc(size_t nBytes,
                ImageEffect *handle = 0) throw(std::bad_alloc);

    /** @brief free n previously allocated memory */
    void free(void *ptr) throw();

  };

};

// undeclare the protected assign and CC macro
#undef mDeclareProtectedAssignAndCC

#endif