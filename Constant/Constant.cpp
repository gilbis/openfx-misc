/*
 OFX ColorCorrect plugin.

 Copyright (C) 2014 INRIA

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France


 The skeleton for this source file is from:
 OFX Basic Example plugin, a plugin that illustrates the use of the OFX Support library.

 Copyright (C) 2004-2005 The Open Effects Association Ltd
 Author Bruno Nicoletti bruno@thefoundry.co.uk

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
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

 The Open Effects Association Ltd
 1 Wardour St
 London W1D 6PA
 England

 */

#include "Constant.h"

#include <cmath>
#ifdef _WINDOWS
#include <windows.h>
#endif
#include <climits>

#include "ofxsProcessing.H"
#include "ofxsMacros.h"
#include "ofxsGenerator.h"

#define kPluginName "ConstantOFX"
#define kPluginGrouping "Image"
#define kPluginDescription "Generate an image with a constant color. A frame range may be specified for operators that need it."
#define kPluginIdentifier "net.sf.openfx.ConstantPlugin"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kRenderThreadSafety eRenderFullySafe

#define kParamColor "color"
#define kParamColorLabel "Color"
#define kParamColorHint "Color to fill the image with."

#define kParamRange "frameRange"
#define kParamRangeLabel "Frame Range"
#define kParamRangeHint "Time domain."

#include "ofxNatron.h"

static bool gHostIsNatron   = false;


/** @brief  Base class used to blend two images together */
class ConstantGeneratorBase : public OFX::ImageProcessor {
protected:
    OfxRGBAColourD _color;

public:
    /** @brief no arg ctor */
    ConstantGeneratorBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    {
        _color.r = _color.g = _color.b = _color.a = 0.;
    }

    /** @brief set the color */
    void setColor(const OfxRGBAColourD& color) {_color = color;}
};

template<int max>
static int floatToInt(float value)
{
    if (value <= 0) {
        return 0;
    } else if (value >= 1.) {
        return max;
    }
    return value * max + 0.5;
}

static inline float to_func_srgb(float v)
{
    if (v < 0.0031308f) {
        return (v < 0.0f) ? 0.0f : v * 12.92f;
    } else {
        return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    }
}


/** @brief templated class to blend between two images */
template <class PIX, int nComponents, int max>
class ConstantGenerator : public ConstantGeneratorBase
{
public:
    // ctor
    ConstantGenerator(OFX::ImageEffect &instance)
    : ConstantGeneratorBase(instance)
    {
    }

private:
    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float colorf[nComponents];
        if (nComponents == 1) {
            // alpha
            colorf[0] = _color.a;
        } else if (nComponents == 3) {
            // rgb
            colorf[0] = _color.r;
            colorf[1] = _color.g;
            colorf[2] = _color.b;
        } else {
            assert(nComponents == 4);
            // rgba
            colorf[0] = _color.r;
            colorf[1] = _color.g;
            colorf[2] = _color.b;
            colorf[3] = _color.a;
        }


        PIX color[nComponents];
        if (max == 1) { // implies float, don't clamp
            for (int c = 0; c < nComponents; ++c) {
                color[c] = colorf[c];
            }
        } else {
            // color is supposed to be linear: delinearize first
            if (nComponents == 3 || nComponents == 4) {
                for (int c = 0; c < nComponents; ++c) {
                    colorf[c] = to_func_srgb(colorf[c]);
                }
            }
            // clamp and convert to the destination type
            for (int c = 0; c < nComponents; ++c) {
                color[c] = floatToInt<max>(colorf[c]);
            }
        }

        // push pixels
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }
            
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                for (int c = 0; c < nComponents; ++c) {
                    dstPix[c] = color[c];
                }
                dstPix += nComponents;
            }
        }
    }

};

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class ConstantPlugin : public GeneratorPlugin
{
protected:
 
    OFX::RGBAParam  *color_;
    OFX::Int2DParam  *range_;

public:
    /** @brief ctor */
    ConstantPlugin(OfxImageEffectHandle handle)
    : GeneratorPlugin(handle)
    , color_(0)
    , range_(0)
    {
        
        color_   = fetchRGBAParam(kParamColor);
        range_   = fetchInt2DParam(kParamRange);
        assert(color_ && range_);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    /* override the time domain action, only for the general context */
    virtual bool getTimeDomain(OfxRangeD &range) OVERRIDE FINAL;

    /* set up and run a processor */
    void setupAndProcess(ConstantGeneratorBase &, const OFX::RenderArguments &args);
    
    virtual bool isIdentity(const OFX::IsIdentityArguments &args,
                               OFX::Clip * &identityClip,
                               double &identityTime) OVERRIDE FINAL;
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from


/* set up and run a processor */
void
ConstantPlugin::setupAndProcess(ConstantGeneratorBase &processor, const OFX::RenderArguments &args)
{
    // get a dst image
    std::auto_ptr<OFX::Image>  dst(dstClip_->fetchImage(args.time));
    //OFX::BitDepthEnum         dstBitDepth    = dst->getPixelDepth();
    //OFX::PixelComponentEnum   dstComponents  = dst->getPixelComponents();

    // set the images
    processor.setDstImg(dst.get());

    // set the render window
    processor.setRenderWindow(args.renderWindow);

    OfxRGBAColourD color;
    color_->getValueAtTime(args.time, color.r, color.g, color.b, color.a);

    processor.setColor(color);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

// the overridden render function
void
ConstantPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = dstClip_->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dstClip_->getPixelComponents();

    // do the rendering
    if (dstComponents == OFX::ePixelComponentRGBA) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                ConstantGenerator<unsigned char, 4, 255> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthUShort: {
                ConstantGenerator<unsigned short, 4, 65535> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthFloat: {
                ConstantGenerator<float, 4, 1> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else if (dstComponents == OFX::ePixelComponentRGB) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                ConstantGenerator<unsigned char, 3, 255> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthUShort: {
                ConstantGenerator<unsigned short, 3, 65535> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthFloat: {
                ConstantGenerator<float, 3, 1> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
        switch (dstBitDepth)
        {
            case OFX::eBitDepthUByte: {
                ConstantGenerator<unsigned char, 1, 255> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthUShort: {
                ConstantGenerator<unsigned short, 1, 65535> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthFloat: {
                ConstantGenerator<float, 1, 1> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
}

/* override the time domain action, only for the general context */
bool
ConstantPlugin::getTimeDomain(OfxRangeD &range)
{
    // this should only be called in the general context, ever!
    if (getContext() == OFX::eContextGeneral) {
        // how many frames on the input clip
        //OfxRangeD srcRange = srcClip_->getFrameRange();

        int min, max;
        range_->getValue(min, max);
        range.min = min;
        range.max = max;
        return true;
    }

    return false;
}

bool
ConstantPlugin::isIdentity(const OFX::IsIdentityArguments &args,
                                OFX::Clip * &identityClip,
                                double &identityTime)
{
    
    if (gHostIsNatron) {
        
        // only Natron supports setting the identityClip to the output clip
        
        int min, max;
        range_->getValue(min, max);
        
        int type_i;
        _type->getValue(type_i);
        GeneratorTypeEnum type = (GeneratorTypeEnum)type_i;
        if (type == eGeneratorTypeSize) {
            ///If not animated and different than 'min' time, return identity on the min time.
            ///We need to check more parameters
            if (color_->getNumKeys() == 0 && _size->getNumKeys() == 0 && _btmLeft->getNumKeys() == 0 && args.time != min) {
                identityClip = dstClip_;
                identityTime = min;
                return true;
            }
        } else {
            ///If not animated and different than 'min' time, return identity on the min time.
            if (color_->getNumKeys() == 0 && args.time != min) {
                identityClip = dstClip_;
                identityTime = min;
                return true;
            }
        }
        
        
       
    }
    return false;
}

using namespace OFX;

mDeclarePluginFactory(ConstantPluginFactory, {}, {});

void ConstantPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextGenerator);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
    desc.setRenderTwiceAlways(false);
    desc.setRenderThreadSafety(kRenderThreadSafety);
    
    generatorDescribeInteract(desc);
}

void ConstantPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, ContextEnum context)
{
    gHostIsNatron = (OFX::getImageEffectHostDescription()->hostName == kOfxNatronHostName);
    
    // there has to be an input clip, even for generators
    ClipDescriptor* srcClip = desc.defineClip( kOfxImageEffectSimpleSourceClipName );
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setOptional(true);

    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);
    dstClip->setFieldExtraction(eFieldExtractSingle);
    
    PageParamDescriptor *page = desc.definePageParam("Controls");
    
    generatorDescribeInContext(page, desc, context);

    // color
    {
        RGBAParamDescriptor* param = desc.defineRGBAParam(kParamColor);
        param->setLabels(kParamColorLabel, kParamColorLabel, kParamColorLabel);
        param->setHint(kParamColorHint);
        param->setDefault(0.0, 0.0, 0.0, 1.0);
        param->setRange(INT_MIN, INT_MIN, INT_MIN, INT_MIN, INT_MAX, INT_MAX, INT_MAX, INT_MAX);
        param->setDisplayRange(0, 0, 0, 0, 1, 1, 1, 1);
        param->setAnimates(true); // can animate
        page->addChild(*param);
    }

    // range
    {
        Int2DParamDescriptor *param = desc.defineInt2DParam(kParamRange);
        param->setLabels(kParamRangeLabel, kParamRangeLabel, kParamRangeLabel);
        param->setHint(kParamRangeHint);
        param->setDefault(1, 1);
        param->setDimensionLabels("min", "max");
        param->setAnimates(false); // can not animate, because it defines the time domain
        page->addChild(*param);
    }
}

ImageEffect* ConstantPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new ConstantPlugin(handle);
}

void getConstantPluginID(OFX::PluginFactoryArray &ids)
{
    static ConstantPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}
