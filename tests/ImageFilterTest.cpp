/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkBlurImageFilter.h"
#include "SkCanvas.h"
#include "SkColorFilterImageFilter.h"
#include "SkColorMatrixFilter.h"
#include "SkComposeImageFilter.h"
#include "SkDisplacementMapEffect.h"
#include "SkDropShadowImageFilter.h"
#include "SkFlattenableSerialization.h"
#include "SkGradientShader.h"
#include "SkImage.h"
#include "SkImageSource.h"
#include "SkLightingImageFilter.h"
#include "SkMatrixConvolutionImageFilter.h"
#include "SkMergeImageFilter.h"
#include "SkMorphologyImageFilter.h"
#include "SkOffsetImageFilter.h"
#include "SkPaintImageFilter.h"
#include "SkPerlinNoiseShader.h"
#include "SkPicture.h"
#include "SkPictureImageFilter.h"
#include "SkPictureRecorder.h"
#include "SkPoint3.h"
#include "SkReadBuffer.h"
#include "SkRect.h"
#include "SkSpecialImage.h"
#include "SkSpecialSurface.h"
#include "SkSurface.h"
#include "SkTableColorFilter.h"
#include "SkTileImageFilter.h"
#include "SkXfermodeImageFilter.h"
#include "Test.h"

#if SK_SUPPORT_GPU
#include "GrContext.h"
#endif

static const int kBitmapSize = 4;

namespace {

class MatrixTestImageFilter : public SkImageFilter {
public:
    static sk_sp<SkImageFilter> Make(skiatest::Reporter* reporter,
                                     const SkMatrix& expectedMatrix) {
        return sk_sp<SkImageFilter>(new MatrixTestImageFilter(reporter, expectedMatrix));
    }

    SK_TO_STRING_OVERRIDE()
    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(MatrixTestImageFilter)

protected:
    sk_sp<SkSpecialImage> onFilterImage(SkSpecialImage* source, const Context& ctx,
                                        SkIPoint* offset) const override {
        REPORTER_ASSERT(fReporter, ctx.ctm() == fExpectedMatrix);
        offset->fX = offset->fY = 0;
        return sk_ref_sp<SkSpecialImage>(source);
    }

    void flatten(SkWriteBuffer& buffer) const override {
        this->INHERITED::flatten(buffer);
        buffer.writeFunctionPtr(fReporter);
        buffer.writeMatrix(fExpectedMatrix);
    }

private:
    MatrixTestImageFilter(skiatest::Reporter* reporter, const SkMatrix& expectedMatrix)
        : INHERITED(nullptr, 0, nullptr)
        , fReporter(reporter)
        , fExpectedMatrix(expectedMatrix) {
    }

    skiatest::Reporter* fReporter;
    SkMatrix fExpectedMatrix;

    typedef SkImageFilter INHERITED;
};

class FailImageFilter : public SkImageFilter {
public:
    FailImageFilter() : SkImageFilter(nullptr, 0, nullptr) { }

    sk_sp<SkSpecialImage> onFilterImage(SkSpecialImage* source,
                                        const Context& ctx,
                                        SkIPoint* offset) const override {
        return nullptr;
    }

    SK_TO_STRING_OVERRIDE()
    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(FailImageFilter)

private:
    typedef SkImageFilter INHERITED;
};

sk_sp<SkFlattenable> FailImageFilter::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 0);
    return sk_sp<SkFlattenable>(new FailImageFilter());
}

#ifndef SK_IGNORE_TO_STRING
void FailImageFilter::toString(SkString* str) const {
    str->appendf("FailImageFilter: (");
    str->append(")");
}
#endif

void draw_gradient_circle(SkCanvas* canvas, int width, int height) {
    SkScalar x = SkIntToScalar(width / 2);
    SkScalar y = SkIntToScalar(height / 2);
    SkScalar radius = SkMinScalar(x, y) * 0.8f;
    canvas->clear(0x00000000);
    SkColor colors[2];
    colors[0] = SK_ColorWHITE;
    colors[1] = SK_ColorBLACK;
    sk_sp<SkShader> shader(
        SkGradientShader::MakeRadial(SkPoint::Make(x, y), radius, colors, nullptr, 2,
                                       SkShader::kClamp_TileMode)
    );
    SkPaint paint;
    paint.setShader(shader);
    canvas->drawCircle(x, y, radius, paint);
}

SkBitmap make_gradient_circle(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    SkCanvas canvas(bitmap);
    draw_gradient_circle(&canvas, width, height);
    return bitmap;
}

class FilterList {
public:
    FilterList(sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect = nullptr) {
        SkPoint3 location = SkPoint3::Make(0, 0, SK_Scalar1);
        const SkScalar five = SkIntToScalar(5);

        {
            sk_sp<SkColorFilter> cf(SkColorFilter::MakeModeFilter(SK_ColorRED,
                                                                  SkXfermode::kSrcIn_Mode));

            this->addFilter("color filter",
                SkColorFilterImageFilter::Make(std::move(cf), input, cropRect));
        }

        {
            sk_sp<SkImage> gradientImage(SkImage::MakeFromBitmap(make_gradient_circle(64, 64)));
            sk_sp<SkImageFilter> gradientSource(SkImageSource::Make(std::move(gradientImage)));

            this->addFilter("displacement map", 
                SkDisplacementMapEffect::Make(SkDisplacementMapEffect::kR_ChannelSelectorType,
                                              SkDisplacementMapEffect::kB_ChannelSelectorType,
                                              20.0f,
                                              std::move(gradientSource), input, cropRect));
        }

        this->addFilter("blur", SkBlurImageFilter::Make(SK_Scalar1,
                                                        SK_Scalar1,
                                                        input,
                                                        cropRect));
        this->addFilter("drop shadow", SkDropShadowImageFilter::Make(
                  SK_Scalar1, SK_Scalar1, SK_Scalar1, SK_Scalar1, SK_ColorGREEN,
                  SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode,
                  input, cropRect));
        this->addFilter("diffuse lighting",
                  SkLightingImageFilter::MakePointLitDiffuse(location, SK_ColorGREEN, 0, 0,
                                                             input, cropRect));
        this->addFilter("specular lighting",
                  SkLightingImageFilter::MakePointLitSpecular(location, SK_ColorGREEN, 0, 0, 0,
                                                              input, cropRect));
        {
            SkScalar kernel[9] = {
                SkIntToScalar(1), SkIntToScalar(1), SkIntToScalar(1),
                SkIntToScalar(1), SkIntToScalar(-7), SkIntToScalar(1),
                SkIntToScalar(1), SkIntToScalar(1), SkIntToScalar(1),
            };
            const SkISize kernelSize = SkISize::Make(3, 3);
            const SkScalar gain = SK_Scalar1, bias = 0;

            this->addFilter("matrix convolution",
                  SkMatrixConvolutionImageFilter::Make(
                      kernelSize, kernel, gain, bias, SkIPoint::Make(1, 1),
                      SkMatrixConvolutionImageFilter::kRepeat_TileMode, false,
                      input, cropRect));
        }

        this->addFilter("merge", SkMergeImageFilter::Make(input, input,
                                                          SkXfermode::kSrcOver_Mode,
                                                          cropRect));

        {
            SkPaint greenColorShaderPaint;
            greenColorShaderPaint.setShader(SkShader::MakeColorShader(SK_ColorGREEN));

            SkImageFilter::CropRect leftSideCropRect(SkRect::MakeXYWH(0, 0, 32, 64));
            sk_sp<SkImageFilter> paintFilterLeft(SkPaintImageFilter::Make(greenColorShaderPaint,
                                                                          &leftSideCropRect));
            SkImageFilter::CropRect rightSideCropRect(SkRect::MakeXYWH(32, 0, 32, 64));
            sk_sp<SkImageFilter> paintFilterRight(SkPaintImageFilter::Make(greenColorShaderPaint,
                                                                           &rightSideCropRect));


            this->addFilter("merge with disjoint inputs", SkMergeImageFilter::Make(
                  std::move(paintFilterLeft), std::move(paintFilterRight),
                  SkXfermode::kSrcOver_Mode, cropRect));
        }

        this->addFilter("offset",
                        SkOffsetImageFilter::Make(SK_Scalar1, SK_Scalar1, input,
                                                  cropRect));
        this->addFilter("dilate", SkDilateImageFilter::Make(3, 2, input, cropRect));
        this->addFilter("erode", SkErodeImageFilter::Make(2, 3, input, cropRect));
        this->addFilter("tile", SkTileImageFilter::Make(
                                    SkRect::MakeXYWH(0, 0, 50, 50),
                                    cropRect ? cropRect->rect() : SkRect::MakeXYWH(0, 0, 100, 100),
                                    input));

        if (!cropRect) {
            SkMatrix matrix;

            matrix.setTranslate(SK_Scalar1, SK_Scalar1);
            matrix.postRotate(SkIntToScalar(45), SK_Scalar1, SK_Scalar1);

            this->addFilter("matrix",
                SkImageFilter::MakeMatrixFilter(matrix, kLow_SkFilterQuality, input));
        }
        {
            sk_sp<SkImageFilter> blur(SkBlurImageFilter::Make(five, five, input));

            this->addFilter("blur and offset", SkOffsetImageFilter::Make(five, five,
                                                                         std::move(blur),
                                                                         cropRect));
        }
        {
            SkRTreeFactory factory;
            SkPictureRecorder recorder;
            SkCanvas* recordingCanvas = recorder.beginRecording(64, 64, &factory, 0);

            SkPaint greenPaint;
            greenPaint.setColor(SK_ColorGREEN);
            recordingCanvas->drawRect(SkRect::Make(SkIRect::MakeXYWH(10, 10, 30, 20)), greenPaint);
            sk_sp<SkPicture> picture(recorder.finishRecordingAsPicture());
            sk_sp<SkImageFilter> pictureFilter(SkPictureImageFilter::Make(std::move(picture)));

            this->addFilter("picture and blur", SkBlurImageFilter::Make(five, five,
                                                                        std::move(pictureFilter),
                                                                        cropRect));
        }
        {
            SkPaint paint;
            paint.setShader(SkPerlinNoiseShader::MakeTurbulence(SK_Scalar1, SK_Scalar1, 1, 0));
            sk_sp<SkImageFilter> paintFilter(SkPaintImageFilter::Make(paint));

            this->addFilter("paint and blur", SkBlurImageFilter::Make(five, five,
                                                                      std::move(paintFilter),
                                                                      cropRect));
        }
        this->addFilter("xfermode", SkXfermodeImageFilter::Make(
            SkXfermode::Make(SkXfermode::kSrc_Mode), input, input, cropRect));
    }
    int count() const { return fFilters.count(); }
    SkImageFilter* getFilter(int index) const { return fFilters[index].fFilter.get(); }
    const char* getName(int index) const { return fFilters[index].fName; }
private:
    struct Filter {
        Filter() : fName(nullptr) {}
        Filter(const char* name, sk_sp<SkImageFilter> filter)
            : fName(name)
            , fFilter(std::move(filter)) {
        }
        const char*                 fName;
        sk_sp<SkImageFilter>        fFilter;
    };
    void addFilter(const char* name, sk_sp<SkImageFilter> filter) {
        fFilters.push_back(Filter(name, std::move(filter)));
    }

    SkTArray<Filter> fFilters;
};

}

sk_sp<SkFlattenable> MatrixTestImageFilter::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 1);
    skiatest::Reporter* reporter = (skiatest::Reporter*)buffer.readFunctionPtr();
    SkMatrix matrix;
    buffer.readMatrix(&matrix);
    return MatrixTestImageFilter::Make(reporter, matrix);
}

#ifndef SK_IGNORE_TO_STRING
void MatrixTestImageFilter::toString(SkString* str) const {
    str->appendf("MatrixTestImageFilter: (");
    str->append(")");
}
#endif

static sk_sp<SkImage> make_small_image() {
    auto surface(SkSurface::MakeRasterN32Premul(kBitmapSize, kBitmapSize));
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(0x00000000);
    SkPaint darkPaint;
    darkPaint.setColor(0xFF804020);
    SkPaint lightPaint;
    lightPaint.setColor(0xFF244484);
    const int i = kBitmapSize / 4;
    for (int y = 0; y < kBitmapSize; y += i) {
        for (int x = 0; x < kBitmapSize; x += i) {
            canvas->save();
            canvas->translate(SkIntToScalar(x), SkIntToScalar(y));
            canvas->drawRect(SkRect::MakeXYWH(0, 0,
                                             SkIntToScalar(i),
                                             SkIntToScalar(i)), darkPaint);
            canvas->drawRect(SkRect::MakeXYWH(SkIntToScalar(i),
                                             0,
                                             SkIntToScalar(i),
                                             SkIntToScalar(i)), lightPaint);
            canvas->drawRect(SkRect::MakeXYWH(0,
                                             SkIntToScalar(i),
                                             SkIntToScalar(i),
                                             SkIntToScalar(i)), lightPaint);
            canvas->drawRect(SkRect::MakeXYWH(SkIntToScalar(i),
                                             SkIntToScalar(i),
                                             SkIntToScalar(i),
                                             SkIntToScalar(i)), darkPaint);
            canvas->restore();
        }
    }

    return surface->makeImageSnapshot();
}

static sk_sp<SkImageFilter> make_scale(float amount, sk_sp<SkImageFilter> input) {
    SkScalar s = amount;
    SkScalar matrix[20] = { s, 0, 0, 0, 0,
                            0, s, 0, 0, 0,
                            0, 0, s, 0, 0,
                            0, 0, 0, s, 0 };
    sk_sp<SkColorFilter> filter(SkColorFilter::MakeMatrixFilterRowMajor255(matrix));
    return SkColorFilterImageFilter::Make(std::move(filter), std::move(input));
}

static sk_sp<SkImageFilter> make_grayscale(sk_sp<SkImageFilter> input,
                                           const SkImageFilter::CropRect* cropRect) {
    SkScalar matrix[20];
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[5] = matrix[10] = 0.2126f;
    matrix[1] = matrix[6] = matrix[11] = 0.7152f;
    matrix[2] = matrix[7] = matrix[12] = 0.0722f;
    matrix[18] = 1.0f;
    sk_sp<SkColorFilter> filter(SkColorFilter::MakeMatrixFilterRowMajor255(matrix));
    return SkColorFilterImageFilter::Make(std::move(filter), std::move(input), cropRect);
}

static sk_sp<SkImageFilter> make_blue(sk_sp<SkImageFilter> input,
                                      const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkColorFilter> filter(SkColorFilter::MakeModeFilter(SK_ColorBLUE,
                                                              SkXfermode::kSrcIn_Mode));
    return SkColorFilterImageFilter::Make(std::move(filter), std::move(input), cropRect);
}

static sk_sp<SkSpecialSurface> create_empty_special_surface(GrContext* context, int widthHeight) {
    if (context) {
        GrSurfaceDesc desc;
        desc.fConfig = kSkia8888_GrPixelConfig;
        desc.fFlags  = kRenderTarget_GrSurfaceFlag;
        desc.fWidth  = widthHeight;
        desc.fHeight = widthHeight;
        return SkSpecialSurface::MakeRenderTarget(context, desc);
    } else {
        const SkImageInfo info = SkImageInfo::MakeN32(widthHeight, widthHeight,
                                                      kOpaque_SkAlphaType);
        return SkSpecialSurface::MakeRaster(info);
    }
}

static sk_sp<SkSpecialImage> create_empty_special_image(GrContext* context, int widthHeight) {
    sk_sp<SkSpecialSurface> surf(create_empty_special_surface(context, widthHeight));

    SkASSERT(surf);

    SkCanvas* canvas = surf->getCanvas();
    SkASSERT(canvas);

    canvas->clear(0x0);

    return surf->makeImageSnapshot();
}


DEF_TEST(ImageFilter, reporter) {
    {
        // Check that two non-clipping color-matrice-filters concatenate into a single filter.
        sk_sp<SkImageFilter> halfBrightness(make_scale(0.5f, nullptr));
        sk_sp<SkImageFilter> quarterBrightness(make_scale(0.5f, std::move(halfBrightness)));
        REPORTER_ASSERT(reporter, nullptr == quarterBrightness->getInput(0));
        SkColorFilter* cf;
        REPORTER_ASSERT(reporter, quarterBrightness->asColorFilter(&cf));
        REPORTER_ASSERT(reporter, cf->asColorMatrix(nullptr));
        cf->unref();
    }

    {
        // Check that a clipping color-matrice-filter followed by a color-matrice-filters
        // concatenates into a single filter, but not a matrixfilter (due to clamping).
        sk_sp<SkImageFilter> doubleBrightness(make_scale(2.0f, nullptr));
        sk_sp<SkImageFilter> halfBrightness(make_scale(0.5f, std::move(doubleBrightness)));
        REPORTER_ASSERT(reporter, nullptr == halfBrightness->getInput(0));
        SkColorFilter* cf;
        REPORTER_ASSERT(reporter, halfBrightness->asColorFilter(&cf));
        REPORTER_ASSERT(reporter, !cf->asColorMatrix(nullptr));
        cf->unref();
    }

    {
        // Check that a color filter image filter without a crop rect can be
        // expressed as a color filter.
        sk_sp<SkImageFilter> gray(make_grayscale(nullptr, nullptr));
        REPORTER_ASSERT(reporter, true == gray->asColorFilter(nullptr));
    }

    {
        // Check that a colorfilterimage filter without a crop rect but with an input
        // that is another colorfilterimage can be expressed as a colorfilter (composed).
        sk_sp<SkImageFilter> mode(make_blue(nullptr, nullptr));
        sk_sp<SkImageFilter> gray(make_grayscale(std::move(mode), nullptr));
        REPORTER_ASSERT(reporter, true == gray->asColorFilter(nullptr));
    }

    {
        // Test that if we exceed the limit of what ComposeColorFilter can combine, we still
        // can build the DAG and won't assert if we call asColorFilter.
        sk_sp<SkImageFilter> filter(make_blue(nullptr, nullptr));
        const int kWayTooManyForComposeColorFilter = 100;
        for (int i = 0; i < kWayTooManyForComposeColorFilter; ++i) {
            filter = make_blue(filter, nullptr);
            // the first few of these will succeed, but after we hit the internal limit,
            // it will then return false.
            (void)filter->asColorFilter(nullptr);
        }
    }

    {
        // Check that a color filter image filter with a crop rect cannot
        // be expressed as a color filter.
        SkImageFilter::CropRect cropRect(SkRect::MakeXYWH(0, 0, 100, 100));
        sk_sp<SkImageFilter> grayWithCrop(make_grayscale(nullptr, &cropRect));
        REPORTER_ASSERT(reporter, false == grayWithCrop->asColorFilter(nullptr));
    }

    {
        // Check that two non-commutative matrices are concatenated in
        // the correct order.
        SkScalar blueToRedMatrix[20] = { 0 };
        blueToRedMatrix[2] = blueToRedMatrix[18] = SK_Scalar1;
        SkScalar redToGreenMatrix[20] = { 0 };
        redToGreenMatrix[5] = redToGreenMatrix[18] = SK_Scalar1;
        sk_sp<SkColorFilter> blueToRed(SkColorFilter::MakeMatrixFilterRowMajor255(blueToRedMatrix));
        sk_sp<SkImageFilter> filter1(SkColorFilterImageFilter::Make(std::move(blueToRed),
                                                                    nullptr));
        sk_sp<SkColorFilter> redToGreen(SkColorFilter::MakeMatrixFilterRowMajor255(redToGreenMatrix));
        sk_sp<SkImageFilter> filter2(SkColorFilterImageFilter::Make(std::move(redToGreen),
                                                                    std::move(filter1)));

        SkBitmap result;
        result.allocN32Pixels(kBitmapSize, kBitmapSize);

        SkPaint paint;
        paint.setColor(SK_ColorBLUE);
        paint.setImageFilter(std::move(filter2));
        SkCanvas canvas(result);
        canvas.clear(0x0);
        SkRect rect = SkRect::Make(SkIRect::MakeWH(kBitmapSize, kBitmapSize));
        canvas.drawRect(rect, paint);
        uint32_t pixel = *result.getAddr32(0, 0);
        // The result here should be green, since we have effectively shifted blue to green.
        REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);
    }

    {
        // Tests pass by not asserting
        sk_sp<SkImage> image(make_small_image());
        SkBitmap result;
        result.allocN32Pixels(kBitmapSize, kBitmapSize);

        {
            // This tests for :
            // 1 ) location at (0,0,1)
            SkPoint3 location = SkPoint3::Make(0, 0, SK_Scalar1);
            // 2 ) location and target at same value
            SkPoint3 target = SkPoint3::Make(location.fX, location.fY, location.fZ);
            // 3 ) large negative specular exponent value
            SkScalar specularExponent = -1000;

            sk_sp<SkImageFilter> bmSrc(SkImageSource::Make(std::move(image)));
            SkPaint paint;
            paint.setImageFilter(SkLightingImageFilter::MakeSpotLitSpecular(
                    location, target, specularExponent, 180,
                    0xFFFFFFFF, SK_Scalar1, SK_Scalar1, SK_Scalar1,
                    std::move(bmSrc)));
            SkCanvas canvas(result);
            SkRect r = SkRect::MakeWH(SkIntToScalar(kBitmapSize),
                                      SkIntToScalar(kBitmapSize));
            canvas.drawRect(r, paint);
        }
    }
}

static void test_crop_rects(skiatest::Reporter* reporter,
                            GrContext* context) {
    // Check that all filters offset to their absolute crop rect,
    // unaffected by the input crop rect.
    // Tests pass by not asserting.
    sk_sp<SkSpecialImage> srcImg(create_empty_special_image(context, 100));
    SkASSERT(srcImg);

    SkImageFilter::CropRect inputCropRect(SkRect::MakeXYWH(8, 13, 80, 80));
    SkImageFilter::CropRect cropRect(SkRect::MakeXYWH(20, 30, 60, 60));
    sk_sp<SkImageFilter> input(make_grayscale(nullptr, &inputCropRect));

    FilterList filters(input, &cropRect);

    for (int i = 0; i < filters.count(); ++i) {
        SkImageFilter* filter = filters.getFilter(i);
        SkIPoint offset;
        SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(100, 100), nullptr);
        sk_sp<SkSpecialImage> resultImg(filter->filterImage(srcImg.get(), ctx, &offset));
        REPORTER_ASSERT_MESSAGE(reporter, resultImg, filters.getName(i));
        REPORTER_ASSERT_MESSAGE(reporter, offset.fX == 20 && offset.fY == 30, filters.getName(i));
    }
}

static void test_negative_blur_sigma(skiatest::Reporter* reporter,
                                     GrContext* context) {
    // Check that SkBlurImageFilter will accept a negative sigma, either in
    // the given arguments or after CTM application.
    const int width = 32, height = 32;
    const SkScalar five = SkIntToScalar(5);

    sk_sp<SkImageFilter> positiveFilter(SkBlurImageFilter::Make(five, five, nullptr));
    sk_sp<SkImageFilter> negativeFilter(SkBlurImageFilter::Make(-five, five, nullptr));

    SkBitmap gradient = make_gradient_circle(width, height);
    sk_sp<SkSpecialImage> imgSrc(SkSpecialImage::MakeFromRaster(SkIRect::MakeWH(width, height),
                                                                gradient));

    SkIPoint offset;
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(32, 32), nullptr);

    sk_sp<SkSpecialImage> positiveResult1(positiveFilter->filterImage(imgSrc.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, positiveResult1);

    sk_sp<SkSpecialImage> negativeResult1(negativeFilter->filterImage(imgSrc.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, negativeResult1);

    SkMatrix negativeScale;
    negativeScale.setScale(-SK_Scalar1, SK_Scalar1);
    SkImageFilter::Context negativeCTX(negativeScale, SkIRect::MakeWH(32, 32), nullptr);

    sk_sp<SkSpecialImage> negativeResult2(positiveFilter->filterImage(imgSrc.get(),
                                                                      negativeCTX,
                                                                      &offset));
    REPORTER_ASSERT(reporter, negativeResult2);

    sk_sp<SkSpecialImage> positiveResult2(negativeFilter->filterImage(imgSrc.get(),
                                                                      negativeCTX,
                                                                      &offset));
    REPORTER_ASSERT(reporter, positiveResult2);


    SkBitmap positiveResultBM1, positiveResultBM2;
    SkBitmap negativeResultBM1, negativeResultBM2;

    REPORTER_ASSERT(reporter, positiveResult1->getROPixels(&positiveResultBM1));
    REPORTER_ASSERT(reporter, positiveResult2->getROPixels(&positiveResultBM2));
    REPORTER_ASSERT(reporter, negativeResult1->getROPixels(&negativeResultBM1));
    REPORTER_ASSERT(reporter, negativeResult2->getROPixels(&negativeResultBM2));

    SkAutoLockPixels lockP1(positiveResultBM1);
    SkAutoLockPixels lockP2(positiveResultBM2);
    SkAutoLockPixels lockN1(negativeResultBM1);
    SkAutoLockPixels lockN2(negativeResultBM2);
    for (int y = 0; y < height; y++) {
        int diffs = memcmp(positiveResultBM1.getAddr32(0, y),
                           negativeResultBM1.getAddr32(0, y),
                           positiveResultBM1.rowBytes());
        REPORTER_ASSERT(reporter, !diffs);
        if (diffs) {
            break;
        }
        diffs = memcmp(positiveResultBM1.getAddr32(0, y),
                       negativeResultBM2.getAddr32(0, y),
                       positiveResultBM1.rowBytes());
        REPORTER_ASSERT(reporter, !diffs);
        if (diffs) {
            break;
        }
        diffs = memcmp(positiveResultBM1.getAddr32(0, y),
                       positiveResultBM2.getAddr32(0, y),
                       positiveResultBM1.rowBytes());
        REPORTER_ASSERT(reporter, !diffs);
        if (diffs) {
            break;
        }
    }
}

DEF_TEST(ImageFilterNegativeBlurSigma, reporter) {
    test_negative_blur_sigma(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterNegativeBlurSigma_Gpu, reporter, ctxInfo) {
    test_negative_blur_sigma(reporter, ctxInfo.fGrContext);
}
#endif

static void test_zero_blur_sigma(skiatest::Reporter* reporter, GrContext* context) {
    // Check that SkBlurImageFilter with a zero sigma and a non-zero srcOffset works correctly.
    SkImageFilter::CropRect cropRect(SkRect::Make(SkIRect::MakeXYWH(5, 0, 5, 10)));
    sk_sp<SkImageFilter> input(SkOffsetImageFilter::Make(0, 0, nullptr, &cropRect));
    sk_sp<SkImageFilter> filter(SkBlurImageFilter::Make(0, 0, std::move(input), &cropRect));

    sk_sp<SkSpecialSurface> surf(create_empty_special_surface(context, 10));
    surf->getCanvas()->clear(SK_ColorGREEN);
    sk_sp<SkSpecialImage> image(surf->makeImageSnapshot());

    SkIPoint offset;
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(32, 32), nullptr);

    sk_sp<SkSpecialImage> result(filter->filterImage(image.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, offset.fX == 5 && offset.fY == 0);
    REPORTER_ASSERT(reporter, result);
    REPORTER_ASSERT(reporter, result->width() == 5 && result->height() == 10);

    SkBitmap resultBM;

    REPORTER_ASSERT(reporter, result->getROPixels(&resultBM));

    SkAutoLockPixels lock(resultBM);
    for (int y = 0; y < resultBM.height(); y++) {
        for (int x = 0; x < resultBM.width(); x++) {
            bool diff = *resultBM.getAddr32(x, y) != SK_ColorGREEN;
            REPORTER_ASSERT(reporter, !diff);
            if (diff) {
                break;
            }
        }
    }
}

DEF_TEST(ImageFilterZeroBlurSigma, reporter) {
    test_zero_blur_sigma(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterZeroBlurSigma_Gpu, reporter, ctxInfo) {
    test_zero_blur_sigma(reporter, ctxInfo.fGrContext);
}
#endif


// Tests that, even when an upstream filter has returned null (due to failure or clipping), a
// downstream filter that affects transparent black still does so even with a nullptr input.
static void test_fail_affects_transparent_black(skiatest::Reporter* reporter, GrContext* context) {
    sk_sp<FailImageFilter> failFilter(new FailImageFilter());
    sk_sp<SkSpecialImage> source(create_empty_special_image(context, 5));
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeXYWH(0, 0, 1, 1), nullptr);
    sk_sp<SkColorFilter> green(SkColorFilter::MakeModeFilter(SK_ColorGREEN, SkXfermode::kSrc_Mode));
    SkASSERT(green->affectsTransparentBlack());
    sk_sp<SkImageFilter> greenFilter(SkColorFilterImageFilter::Make(std::move(green),
                                                                    std::move(failFilter)));
    SkIPoint offset;
    sk_sp<SkSpecialImage> result(greenFilter->filterImage(source.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, nullptr != result.get());
    if (result.get()) {
        SkBitmap resultBM;
        REPORTER_ASSERT(reporter, result->getROPixels(&resultBM));
        SkAutoLockPixels lock(resultBM);
        REPORTER_ASSERT(reporter, *resultBM.getAddr32(0, 0) == SK_ColorGREEN);
    }
}

DEF_TEST(ImageFilterFailAffectsTransparentBlack, reporter) {
    test_fail_affects_transparent_black(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterFailAffectsTransparentBlack_Gpu, reporter, ctxInfo) {
    test_fail_affects_transparent_black(reporter, ctxInfo.fGrContext);
}
#endif

DEF_TEST(ImageFilterDrawTiled, reporter) {
    // Check that all filters when drawn tiled (with subsequent clip rects) exactly
    // match the same filters drawn with a single full-canvas bitmap draw.
    // Tests pass by not asserting.

    FilterList filters(nullptr);

    SkBitmap untiledResult, tiledResult;
    const int width = 64, height = 64;
    untiledResult.allocN32Pixels(width, height);
    tiledResult.allocN32Pixels(width, height);
    SkCanvas tiledCanvas(tiledResult);
    SkCanvas untiledCanvas(untiledResult);
    int tileSize = 8;

    for (int scale = 1; scale <= 2; ++scale) {
        for (int i = 0; i < filters.count(); ++i) {
            tiledCanvas.clear(0);
            untiledCanvas.clear(0);
            SkPaint paint;
            paint.setImageFilter(filters.getFilter(i));
            paint.setTextSize(SkIntToScalar(height));
            paint.setColor(SK_ColorWHITE);
            SkString str;
            const char* text = "ABC";
            SkScalar ypos = SkIntToScalar(height);
            untiledCanvas.save();
            untiledCanvas.scale(SkIntToScalar(scale), SkIntToScalar(scale));
            untiledCanvas.drawText(text, strlen(text), 0, ypos, paint);
            untiledCanvas.restore();
            for (int y = 0; y < height; y += tileSize) {
                for (int x = 0; x < width; x += tileSize) {
                    tiledCanvas.save();
                    tiledCanvas.clipRect(SkRect::Make(SkIRect::MakeXYWH(x, y, tileSize, tileSize)));
                    tiledCanvas.scale(SkIntToScalar(scale), SkIntToScalar(scale));
                    tiledCanvas.drawText(text, strlen(text), 0, ypos, paint);
                    tiledCanvas.restore();
                }
            }
            untiledCanvas.flush();
            tiledCanvas.flush();
            for (int y = 0; y < height; y++) {
                int diffs = memcmp(untiledResult.getAddr32(0, y), tiledResult.getAddr32(0, y), untiledResult.rowBytes());
                REPORTER_ASSERT_MESSAGE(reporter, !diffs, filters.getName(i));
                if (diffs) {
                    break;
                }
            }
        }
    }
}

static void draw_saveLayer_picture(int width, int height, int tileSize,
                                   SkBBHFactory* factory, SkBitmap* result) {

    SkMatrix matrix;
    matrix.setTranslate(SkIntToScalar(50), 0);

    sk_sp<SkColorFilter> cf(SkColorFilter::MakeModeFilter(SK_ColorWHITE, SkXfermode::kSrc_Mode));
    sk_sp<SkImageFilter> cfif(SkColorFilterImageFilter::Make(std::move(cf), nullptr));
    sk_sp<SkImageFilter> imageFilter(SkImageFilter::MakeMatrixFilter(matrix,
                                                                     kNone_SkFilterQuality,
                                                                     std::move(cfif)));

    SkPaint paint;
    paint.setImageFilter(std::move(imageFilter));
    SkPictureRecorder recorder;
    SkRect bounds = SkRect::Make(SkIRect::MakeXYWH(0, 0, 50, 50));
    SkCanvas* recordingCanvas = recorder.beginRecording(SkIntToScalar(width),
                                                        SkIntToScalar(height),
                                                        factory, 0);
    recordingCanvas->translate(-55, 0);
    recordingCanvas->saveLayer(&bounds, &paint);
    recordingCanvas->restore();
    sk_sp<SkPicture> picture1(recorder.finishRecordingAsPicture());

    result->allocN32Pixels(width, height);
    SkCanvas canvas(*result);
    canvas.clear(0);
    canvas.clipRect(SkRect::Make(SkIRect::MakeWH(tileSize, tileSize)));
    canvas.drawPicture(picture1.get());
}

DEF_TEST(ImageFilterDrawMatrixBBH, reporter) {
    // Check that matrix filter when drawn tiled with BBH exactly
    // matches the same thing drawn without BBH.
    // Tests pass by not asserting.

    const int width = 200, height = 200;
    const int tileSize = 100;
    SkBitmap result1, result2;
    SkRTreeFactory factory;

    draw_saveLayer_picture(width, height, tileSize, &factory, &result1);
    draw_saveLayer_picture(width, height, tileSize, nullptr, &result2);

    for (int y = 0; y < height; y++) {
        int diffs = memcmp(result1.getAddr32(0, y), result2.getAddr32(0, y), result1.rowBytes());
        REPORTER_ASSERT(reporter, !diffs);
        if (diffs) {
            break;
        }
    }
}

static sk_sp<SkImageFilter> make_blur(sk_sp<SkImageFilter> input) {
    return SkBlurImageFilter::Make(SK_Scalar1, SK_Scalar1, std::move(input));
}

static sk_sp<SkImageFilter> make_drop_shadow(sk_sp<SkImageFilter> input) {
    return SkDropShadowImageFilter::Make(
        SkIntToScalar(100), SkIntToScalar(100),
        SkIntToScalar(10), SkIntToScalar(10),
        SK_ColorBLUE, SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode,
        std::move(input));
}

DEF_TEST(ImageFilterBlurThenShadowBounds, reporter) {
    sk_sp<SkImageFilter> filter1(make_blur(nullptr));
    sk_sp<SkImageFilter> filter2(make_drop_shadow(std::move(filter1)));

    SkIRect bounds = SkIRect::MakeXYWH(0, 0, 100, 100);
    SkIRect expectedBounds = SkIRect::MakeXYWH(-133, -133, 236, 236);
    bounds = filter2->filterBounds(bounds, SkMatrix::I());

    REPORTER_ASSERT(reporter, bounds == expectedBounds);
}

DEF_TEST(ImageFilterShadowThenBlurBounds, reporter) {
    sk_sp<SkImageFilter> filter1(make_drop_shadow(nullptr));
    sk_sp<SkImageFilter> filter2(make_blur(std::move(filter1)));

    SkIRect bounds = SkIRect::MakeXYWH(0, 0, 100, 100);
    SkIRect expectedBounds = SkIRect::MakeXYWH(-133, -133, 236, 236);
    bounds = filter2->filterBounds(bounds, SkMatrix::I());

    REPORTER_ASSERT(reporter, bounds == expectedBounds);
}

DEF_TEST(ImageFilterDilateThenBlurBounds, reporter) {
    sk_sp<SkImageFilter> filter1(SkDilateImageFilter::Make(2, 2, nullptr));
    sk_sp<SkImageFilter> filter2(make_drop_shadow(std::move(filter1)));

    SkIRect bounds = SkIRect::MakeXYWH(0, 0, 100, 100);
    SkIRect expectedBounds = SkIRect::MakeXYWH(-132, -132, 234, 234);
    bounds = filter2->filterBounds(bounds, SkMatrix::I());

    REPORTER_ASSERT(reporter, bounds == expectedBounds);
}

DEF_TEST(ImageFilterComposedBlurFastBounds, reporter) {
    sk_sp<SkImageFilter> filter1(make_blur(nullptr));
    sk_sp<SkImageFilter> filter2(make_blur(nullptr));
    sk_sp<SkImageFilter> composedFilter(SkComposeImageFilter::Make(std::move(filter1),
                                                                   std::move(filter2)));

    SkRect boundsSrc = SkRect::MakeWH(SkIntToScalar(100), SkIntToScalar(100));
    SkRect expectedBounds = SkRect::MakeXYWH(
        SkIntToScalar(-6), SkIntToScalar(-6), SkIntToScalar(112), SkIntToScalar(112));
    SkRect boundsDst = composedFilter->computeFastBounds(boundsSrc);

    REPORTER_ASSERT(reporter, boundsDst == expectedBounds);
}

DEF_TEST(ImageFilterUnionBounds, reporter) {
    sk_sp<SkImageFilter> offset(SkOffsetImageFilter::Make(50, 0, nullptr));
    // Regardless of which order they appear in, the image filter bounds should
    // be combined correctly.
    {
        sk_sp<SkImageFilter> composite(SkXfermodeImageFilter::Make(nullptr, offset));
        SkRect bounds = SkRect::MakeWH(100, 100);
        // Intentionally aliasing here, as that's what the real callers do.
        bounds = composite->computeFastBounds(bounds);
        REPORTER_ASSERT(reporter, bounds == SkRect::MakeWH(150, 100));
    }
    {
        sk_sp<SkImageFilter> composite(SkXfermodeImageFilter::Make(nullptr, nullptr,
                                                                   offset, nullptr));
        SkRect bounds = SkRect::MakeWH(100, 100);
        // Intentionally aliasing here, as that's what the real callers do.
        bounds = composite->computeFastBounds(bounds);
        REPORTER_ASSERT(reporter, bounds == SkRect::MakeWH(150, 100));
    }
}

static void test_imagefilter_merge_result_size(skiatest::Reporter* reporter, GrContext* context) {
    SkBitmap greenBM;
    greenBM.allocN32Pixels(20, 20);
    greenBM.eraseColor(SK_ColorGREEN);
    sk_sp<SkImage> greenImage(SkImage::MakeFromBitmap(greenBM));
    sk_sp<SkImageFilter> source(SkImageSource::Make(std::move(greenImage)));
    sk_sp<SkImageFilter> merge(SkMergeImageFilter::Make(source, source));

    sk_sp<SkSpecialImage> srcImg(create_empty_special_image(context, 1));

    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeXYWH(0, 0, 100, 100), nullptr);
    SkIPoint offset;

    sk_sp<SkSpecialImage> resultImg(merge->filterImage(srcImg.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, resultImg);

    REPORTER_ASSERT(reporter, resultImg->width() == 20 && resultImg->height() == 20);
}

DEF_TEST(ImageFilterMergeResultSize, reporter) {
    test_imagefilter_merge_result_size(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(ImageFilterMergeResultSize_Gpu, reporter, ctxInfo) {
    test_imagefilter_merge_result_size(reporter, ctxInfo.fGrContext);
}
#endif

static void draw_blurred_rect(SkCanvas* canvas) {
    SkPaint filterPaint;
    filterPaint.setColor(SK_ColorWHITE);
    filterPaint.setImageFilter(SkBlurImageFilter::Make(SkIntToScalar(8), 0, nullptr));
    canvas->saveLayer(nullptr, &filterPaint);
    SkPaint whitePaint;
    whitePaint.setColor(SK_ColorWHITE);
    canvas->drawRect(SkRect::Make(SkIRect::MakeWH(4, 4)), whitePaint);
    canvas->restore();
}

static void draw_picture_clipped(SkCanvas* canvas, const SkRect& clipRect, const SkPicture* picture) {
    canvas->save();
    canvas->clipRect(clipRect);
    canvas->drawPicture(picture);
    canvas->restore();
}

DEF_TEST(ImageFilterDrawTiledBlurRTree, reporter) {
    // Check that the blur filter when recorded with RTree acceleration,
    // and drawn tiled (with subsequent clip rects) exactly
    // matches the same filter drawn with without RTree acceleration.
    // This tests that the "bleed" from the blur into the otherwise-blank
    // tiles is correctly rendered.
    // Tests pass by not asserting.

    int width = 16, height = 8;
    SkBitmap result1, result2;
    result1.allocN32Pixels(width, height);
    result2.allocN32Pixels(width, height);
    SkCanvas canvas1(result1);
    SkCanvas canvas2(result2);
    int tileSize = 8;

    canvas1.clear(0);
    canvas2.clear(0);

    SkRTreeFactory factory;

    SkPictureRecorder recorder1, recorder2;
    // The only difference between these two pictures is that one has RTree aceleration.
    SkCanvas* recordingCanvas1 = recorder1.beginRecording(SkIntToScalar(width),
                                                          SkIntToScalar(height),
                                                          nullptr, 0);
    SkCanvas* recordingCanvas2 = recorder2.beginRecording(SkIntToScalar(width),
                                                          SkIntToScalar(height),
                                                          &factory, 0);
    draw_blurred_rect(recordingCanvas1);
    draw_blurred_rect(recordingCanvas2);
    sk_sp<SkPicture> picture1(recorder1.finishRecordingAsPicture());
    sk_sp<SkPicture> picture2(recorder2.finishRecordingAsPicture());
    for (int y = 0; y < height; y += tileSize) {
        for (int x = 0; x < width; x += tileSize) {
            SkRect tileRect = SkRect::Make(SkIRect::MakeXYWH(x, y, tileSize, tileSize));
            draw_picture_clipped(&canvas1, tileRect, picture1.get());
            draw_picture_clipped(&canvas2, tileRect, picture2.get());
        }
    }
    for (int y = 0; y < height; y++) {
        int diffs = memcmp(result1.getAddr32(0, y), result2.getAddr32(0, y), result1.rowBytes());
        REPORTER_ASSERT(reporter, !diffs);
        if (diffs) {
            break;
        }
    }
}

DEF_TEST(ImageFilterMatrixConvolution, reporter) {
    // Check that a 1x3 filter does not cause a spurious assert.
    SkScalar kernel[3] = {
        SkIntToScalar( 1), SkIntToScalar( 1), SkIntToScalar( 1),
    };
    SkISize kernelSize = SkISize::Make(1, 3);
    SkScalar gain = SK_Scalar1, bias = 0;
    SkIPoint kernelOffset = SkIPoint::Make(0, 0);

    sk_sp<SkImageFilter> filter(SkMatrixConvolutionImageFilter::Make(
                                            kernelSize, kernel,
                                            gain, bias, kernelOffset,
                                            SkMatrixConvolutionImageFilter::kRepeat_TileMode,
                                            false, nullptr));

    SkBitmap result;
    int width = 16, height = 16;
    result.allocN32Pixels(width, height);
    SkCanvas canvas(result);
    canvas.clear(0);

    SkPaint paint;
    paint.setImageFilter(std::move(filter));
    SkRect rect = SkRect::Make(SkIRect::MakeWH(width, height));
    canvas.drawRect(rect, paint);
}

DEF_TEST(ImageFilterMatrixConvolutionBorder, reporter) {
    // Check that a filter with borders outside the target bounds
    // does not crash.
    SkScalar kernel[3] = {
        0, 0, 0,
    };
    SkISize kernelSize = SkISize::Make(3, 1);
    SkScalar gain = SK_Scalar1, bias = 0;
    SkIPoint kernelOffset = SkIPoint::Make(2, 0);

    sk_sp<SkImageFilter> filter(SkMatrixConvolutionImageFilter::Make(
                                            kernelSize, kernel, gain, bias, kernelOffset,
                                            SkMatrixConvolutionImageFilter::kClamp_TileMode,
                                            true, nullptr));

    SkBitmap result;

    int width = 10, height = 10;
    result.allocN32Pixels(width, height);
    SkCanvas canvas(result);
    canvas.clear(0);

    SkPaint filterPaint;
    filterPaint.setImageFilter(std::move(filter));
    SkRect bounds = SkRect::MakeWH(1, 10);
    SkRect rect = SkRect::Make(SkIRect::MakeWH(width, height));
    SkPaint rectPaint;
    canvas.saveLayer(&bounds, &filterPaint);
    canvas.drawRect(rect, rectPaint);
    canvas.restore();
}

static void test_big_kernel(skiatest::Reporter* reporter, GrContext* context) {
    // Check that a kernel that is too big for the GPU still works
    SkScalar identityKernel[49] = {
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0
    };
    SkISize kernelSize = SkISize::Make(7, 7);
    SkScalar gain = SK_Scalar1, bias = 0;
    SkIPoint kernelOffset = SkIPoint::Make(0, 0);

    sk_sp<SkImageFilter> filter(SkMatrixConvolutionImageFilter::Make(
                                        kernelSize, identityKernel, gain, bias, kernelOffset,
                                        SkMatrixConvolutionImageFilter::kClamp_TileMode,
                                        true, nullptr));

    sk_sp<SkSpecialImage> srcImg(create_empty_special_image(context, 100));
    SkASSERT(srcImg);

    SkIPoint offset;
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(100, 100), nullptr);
    sk_sp<SkSpecialImage> resultImg(filter->filterImage(srcImg.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, resultImg);
    REPORTER_ASSERT(reporter, SkToBool(context) == resultImg->isTextureBacked());
    REPORTER_ASSERT(reporter, resultImg->width() == 100 && resultImg->height() == 100);
    REPORTER_ASSERT(reporter, offset.fX == 0 && offset.fY == 0);
}

DEF_TEST(ImageFilterMatrixConvolutionBigKernel, reporter) {
    test_big_kernel(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(ImageFilterMatrixConvolutionBigKernel_Gpu,
                                      reporter, ctxInfo) {
    test_big_kernel(reporter, ctxInfo.fGrContext);
}
#endif

DEF_TEST(ImageFilterCropRect, reporter) {
    test_crop_rects(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterCropRect_Gpu, reporter, ctxInfo) {
    test_crop_rects(reporter, ctxInfo.fGrContext);
}
#endif

DEF_TEST(ImageFilterMatrix, reporter) {
    SkBitmap temp;
    temp.allocN32Pixels(100, 100);
    SkCanvas canvas(temp);
    canvas.scale(SkIntToScalar(2), SkIntToScalar(2));

    SkMatrix expectedMatrix = canvas.getTotalMatrix();

    SkRTreeFactory factory;
    SkPictureRecorder recorder;
    SkCanvas* recordingCanvas = recorder.beginRecording(100, 100, &factory, 0);

    SkPaint paint;
    paint.setImageFilter(MatrixTestImageFilter::Make(reporter, expectedMatrix));
    recordingCanvas->saveLayer(nullptr, &paint);
    SkPaint solidPaint;
    solidPaint.setColor(0xFFFFFFFF);
    recordingCanvas->save();
    recordingCanvas->scale(SkIntToScalar(10), SkIntToScalar(10));
    recordingCanvas->drawRect(SkRect::Make(SkIRect::MakeWH(100, 100)), solidPaint);
    recordingCanvas->restore(); // scale
    recordingCanvas->restore(); // saveLayer

    canvas.drawPicture(recorder.finishRecordingAsPicture());
}

DEF_TEST(ImageFilterCrossProcessPictureImageFilter, reporter) {
    SkRTreeFactory factory;
    SkPictureRecorder recorder;
    SkCanvas* recordingCanvas = recorder.beginRecording(1, 1, &factory, 0);

    // Create an SkPicture which simply draws a green 1x1 rectangle.
    SkPaint greenPaint;
    greenPaint.setColor(SK_ColorGREEN);
    recordingCanvas->drawRect(SkRect::Make(SkIRect::MakeWH(1, 1)), greenPaint);
    sk_sp<SkPicture> picture(recorder.finishRecordingAsPicture());

    // Wrap that SkPicture in an SkPictureImageFilter.
    sk_sp<SkImageFilter> imageFilter(SkPictureImageFilter::Make(picture));

    // Check that SkPictureImageFilter successfully serializes its contained
    // SkPicture when not in cross-process mode.
    SkPaint paint;
    paint.setImageFilter(imageFilter);
    SkPictureRecorder outerRecorder;
    SkCanvas* outerCanvas = outerRecorder.beginRecording(1, 1, &factory, 0);
    SkPaint redPaintWithFilter;
    redPaintWithFilter.setColor(SK_ColorRED);
    redPaintWithFilter.setImageFilter(imageFilter);
    outerCanvas->drawRect(SkRect::Make(SkIRect::MakeWH(1, 1)), redPaintWithFilter);
    sk_sp<SkPicture> outerPicture(outerRecorder.finishRecordingAsPicture());

    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    SkCanvas canvas(bitmap);

    // The result here should be green, since the filter replaces the primitive's red interior.
    canvas.clear(0x0);
    canvas.drawPicture(outerPicture);
    uint32_t pixel = *bitmap.getAddr32(0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);

    // Check that, for now, SkPictureImageFilter does not serialize or
    // deserialize its contained picture when the filter is serialized
    // cross-process. Do this by "laundering" it through SkValidatingReadBuffer.
    sk_sp<SkData> data(SkValidatingSerializeFlattenable(imageFilter.get()));
    sk_sp<SkFlattenable> flattenable(SkValidatingDeserializeFlattenable(
        data->data(), data->size(), SkImageFilter::GetFlattenableType()));
    SkImageFilter* unflattenedFilter = static_cast<SkImageFilter*>(flattenable.get());

    redPaintWithFilter.setImageFilter(unflattenedFilter);
    SkPictureRecorder crossProcessRecorder;
    SkCanvas* crossProcessCanvas = crossProcessRecorder.beginRecording(1, 1, &factory, 0);
    crossProcessCanvas->drawRect(SkRect::Make(SkIRect::MakeWH(1, 1)), redPaintWithFilter);
    sk_sp<SkPicture> crossProcessPicture(crossProcessRecorder.finishRecordingAsPicture());

    canvas.clear(0x0);
    canvas.drawPicture(crossProcessPicture);
    pixel = *bitmap.getAddr32(0, 0);
    // If the security precautions are enabled, the result here should not be green, since the
    // filter draws nothing.
    REPORTER_ASSERT(reporter, SkPicture::PictureIOSecurityPrecautionsEnabled()
        ? pixel != SK_ColorGREEN : pixel == SK_ColorGREEN);
}

static void test_clipped_picture_imagefilter(skiatest::Reporter* reporter, GrContext* context) {
    sk_sp<SkPicture> picture;

    {
        SkRTreeFactory factory;
        SkPictureRecorder recorder;
        SkCanvas* recordingCanvas = recorder.beginRecording(1, 1, &factory, 0);

        // Create an SkPicture which simply draws a green 1x1 rectangle.
        SkPaint greenPaint;
        greenPaint.setColor(SK_ColorGREEN);
        recordingCanvas->drawRect(SkRect::Make(SkIRect::MakeWH(1, 1)), greenPaint);
        picture = recorder.finishRecordingAsPicture();
    }

    sk_sp<SkSpecialImage> srcImg(create_empty_special_image(context, 2));

    sk_sp<SkImageFilter> imageFilter(SkPictureImageFilter::Make(picture));

    SkIPoint offset;
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeXYWH(1, 1, 1, 1), nullptr);

    sk_sp<SkSpecialImage> resultImage(imageFilter->filterImage(srcImg.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, !resultImage);
}

DEF_TEST(ImageFilterClippedPictureImageFilter, reporter) {
    test_clipped_picture_imagefilter(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterClippedPictureImageFilter_Gpu, reporter, ctxInfo) {
    test_clipped_picture_imagefilter(reporter, ctxInfo.fGrContext);
}
#endif

DEF_TEST(ImageFilterEmptySaveLayer, reporter) {
    // Even when there's an empty saveLayer()/restore(), ensure that an image
    // filter or color filter which affects transparent black still draws.

    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    SkCanvas canvas(bitmap);

    SkRTreeFactory factory;
    SkPictureRecorder recorder;

    sk_sp<SkColorFilter> green(SkColorFilter::MakeModeFilter(SK_ColorGREEN,
                                                             SkXfermode::kSrc_Mode));
    sk_sp<SkImageFilter> imageFilter(SkColorFilterImageFilter::Make(green, nullptr));
    SkPaint imageFilterPaint;
    imageFilterPaint.setImageFilter(std::move(imageFilter));
    SkPaint colorFilterPaint;
    colorFilterPaint.setColorFilter(green);

    SkRect bounds = SkRect::MakeWH(10, 10);

    SkCanvas* recordingCanvas = recorder.beginRecording(10, 10, &factory, 0);
    recordingCanvas->saveLayer(&bounds, &imageFilterPaint);
    recordingCanvas->restore();
    sk_sp<SkPicture> picture(recorder.finishRecordingAsPicture());

    canvas.clear(0);
    canvas.drawPicture(picture);
    uint32_t pixel = *bitmap.getAddr32(0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);

    recordingCanvas = recorder.beginRecording(10, 10, &factory, 0);
    recordingCanvas->saveLayer(nullptr, &imageFilterPaint);
    recordingCanvas->restore();
    sk_sp<SkPicture> picture2(recorder.finishRecordingAsPicture());

    canvas.clear(0);
    canvas.drawPicture(picture2);
    pixel = *bitmap.getAddr32(0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);

    recordingCanvas = recorder.beginRecording(10, 10, &factory, 0);
    recordingCanvas->saveLayer(&bounds, &colorFilterPaint);
    recordingCanvas->restore();
    sk_sp<SkPicture> picture3(recorder.finishRecordingAsPicture());

    canvas.clear(0);
    canvas.drawPicture(picture3);
    pixel = *bitmap.getAddr32(0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);
}

static void test_huge_blur(SkCanvas* canvas, skiatest::Reporter* reporter) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 100);
    bitmap.eraseARGB(0, 0, 0, 0);

    // Check that a blur with an insane radius does not crash or assert.
    SkPaint paint;
    paint.setImageFilter(SkBlurImageFilter::Make(SkIntToScalar(1<<30),
                                                 SkIntToScalar(1<<30),
                                                 nullptr));
    canvas->drawBitmap(bitmap, 0, 0, &paint);
}

DEF_TEST(HugeBlurImageFilter, reporter) {
    SkBitmap temp;
    temp.allocN32Pixels(100, 100);
    SkCanvas canvas(temp);
    test_huge_blur(&canvas, reporter);
}

DEF_TEST(ImageFilterMatrixConvolutionSanityTest, reporter) {
    SkScalar kernel[1] = { 0 };
    SkScalar gain = SK_Scalar1, bias = 0;
    SkIPoint kernelOffset = SkIPoint::Make(1, 1);

    // Check that an enormous (non-allocatable) kernel gives a nullptr filter.
    sk_sp<SkImageFilter> conv(SkMatrixConvolutionImageFilter::Make(
        SkISize::Make(1<<30, 1<<30),
        kernel,
        gain,
        bias,
        kernelOffset,
        SkMatrixConvolutionImageFilter::kRepeat_TileMode,
        false,
        nullptr));

    REPORTER_ASSERT(reporter, nullptr == conv.get());

    // Check that a nullptr kernel gives a nullptr filter.
    conv = SkMatrixConvolutionImageFilter::Make(
        SkISize::Make(1, 1),
        nullptr,
        gain,
        bias,
        kernelOffset,
        SkMatrixConvolutionImageFilter::kRepeat_TileMode,
        false,
        nullptr);

    REPORTER_ASSERT(reporter, nullptr == conv.get());

    // Check that a kernel width < 1 gives a nullptr filter.
    conv = SkMatrixConvolutionImageFilter::Make(
        SkISize::Make(0, 1),
        kernel,
        gain,
        bias,
        kernelOffset,
        SkMatrixConvolutionImageFilter::kRepeat_TileMode,
        false,
        nullptr);

    REPORTER_ASSERT(reporter, nullptr == conv.get());

    // Check that kernel height < 1 gives a nullptr filter.
    conv = SkMatrixConvolutionImageFilter::Make(
        SkISize::Make(1, -1),
        kernel,
        gain,
        bias,
        kernelOffset,
        SkMatrixConvolutionImageFilter::kRepeat_TileMode,
        false,
        nullptr);

    REPORTER_ASSERT(reporter, nullptr == conv.get());
}

static void test_xfermode_cropped_input(SkCanvas* canvas, skiatest::Reporter* reporter) {
    canvas->clear(0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseARGB(255, 255, 255, 255);

    sk_sp<SkColorFilter> green(SkColorFilter::MakeModeFilter(SK_ColorGREEN,
                                                             SkXfermode::kSrcIn_Mode));
    sk_sp<SkImageFilter> greenFilter(SkColorFilterImageFilter::Make(green, nullptr));
    SkImageFilter::CropRect cropRect(SkRect::MakeEmpty());
    sk_sp<SkImageFilter> croppedOut(SkColorFilterImageFilter::Make(green, nullptr, &cropRect));

    // Check that an xfermode image filter whose input has been cropped out still draws the other
    // input. Also check that drawing with both inputs cropped out doesn't cause a GPU warning.
    sk_sp<SkXfermode> mode(SkXfermode::Make(SkXfermode::kSrcOver_Mode));
    sk_sp<SkImageFilter> xfermodeNoFg(SkXfermodeImageFilter::Make(mode, greenFilter,
                                                                  croppedOut, nullptr));
    sk_sp<SkImageFilter> xfermodeNoBg(SkXfermodeImageFilter::Make(mode, croppedOut,
                                                                  greenFilter, nullptr));
    sk_sp<SkImageFilter> xfermodeNoFgNoBg(SkXfermodeImageFilter::Make(mode, croppedOut,
                                                                      croppedOut, nullptr));

    SkPaint paint;
    paint.setImageFilter(std::move(xfermodeNoFg));
    canvas->drawBitmap(bitmap, 0, 0, &paint);   // drawSprite

    uint32_t pixel;
    SkImageInfo info = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
    canvas->readPixels(info, &pixel, 4, 0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);

    paint.setImageFilter(std::move(xfermodeNoBg));
    canvas->drawBitmap(bitmap, 0, 0, &paint);   // drawSprite
    canvas->readPixels(info, &pixel, 4, 0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);

    paint.setImageFilter(std::move(xfermodeNoFgNoBg));
    canvas->drawBitmap(bitmap, 0, 0, &paint);   // drawSprite
    canvas->readPixels(info, &pixel, 4, 0, 0);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);
}

DEF_TEST(ImageFilterNestedSaveLayer, reporter) {
    SkBitmap temp;
    temp.allocN32Pixels(50, 50);
    SkCanvas canvas(temp);
    canvas.clear(0x0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    bitmap.eraseColor(SK_ColorGREEN);

    SkMatrix matrix;
    matrix.setScale(SkIntToScalar(2), SkIntToScalar(2));
    matrix.postTranslate(SkIntToScalar(-20), SkIntToScalar(-20));
    sk_sp<SkImageFilter> matrixFilter(
        SkImageFilter::MakeMatrixFilter(matrix, kLow_SkFilterQuality, nullptr));

    // Test that saveLayer() with a filter nested inside another saveLayer() applies the
    // correct offset to the filter matrix.
    SkRect bounds1 = SkRect::MakeXYWH(10, 10, 30, 30);
    canvas.saveLayer(&bounds1, nullptr);
    SkPaint filterPaint;
    filterPaint.setImageFilter(std::move(matrixFilter));
    SkRect bounds2 = SkRect::MakeXYWH(20, 20, 10, 10);
    canvas.saveLayer(&bounds2, &filterPaint);
    SkPaint greenPaint;
    greenPaint.setColor(SK_ColorGREEN);
    canvas.drawRect(bounds2, greenPaint);
    canvas.restore();
    canvas.restore();
    SkPaint strokePaint;
    strokePaint.setStyle(SkPaint::kStroke_Style);
    strokePaint.setColor(SK_ColorRED);

    SkImageInfo info = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
    uint32_t pixel;
    canvas.readPixels(info, &pixel, 4, 25, 25);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);

    // Test that drawSprite() with a filter nested inside a saveLayer() applies the
    // correct offset to the filter matrix.
    canvas.clear(0x0);
    canvas.readPixels(info, &pixel, 4, 25, 25);
    canvas.saveLayer(&bounds1, nullptr);
    canvas.drawBitmap(bitmap, 20, 20, &filterPaint);    // drawSprite
    canvas.restore();

    canvas.readPixels(info, &pixel, 4, 25, 25);
    REPORTER_ASSERT(reporter, pixel == SK_ColorGREEN);
}

DEF_TEST(XfermodeImageFilterCroppedInput, reporter) {
    SkBitmap temp;
    temp.allocN32Pixels(100, 100);
    SkCanvas canvas(temp);
    test_xfermode_cropped_input(&canvas, reporter);
}

static void test_composed_imagefilter_offset(skiatest::Reporter* reporter, GrContext* context) {
    sk_sp<SkSpecialImage> srcImg(create_empty_special_image(context, 100));

    SkImageFilter::CropRect cropRect(SkRect::MakeXYWH(1, 0, 20, 20));
    sk_sp<SkImageFilter> offsetFilter(SkOffsetImageFilter::Make(0, 0, nullptr, &cropRect));
    sk_sp<SkImageFilter> blurFilter(SkBlurImageFilter::Make(SK_Scalar1, SK_Scalar1,
                                                            nullptr, &cropRect));
    sk_sp<SkImageFilter> composedFilter(SkComposeImageFilter::Make(std::move(blurFilter),
                                                                   std::move(offsetFilter)));
    SkIPoint offset;
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(100, 100), nullptr);

    sk_sp<SkSpecialImage> resultImg(composedFilter->filterImage(srcImg.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, resultImg);
    REPORTER_ASSERT(reporter, offset.fX == 1 && offset.fY == 0);
}

DEF_TEST(ComposedImageFilterOffset, reporter) {
    test_composed_imagefilter_offset(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ComposedImageFilterOffset_Gpu, reporter, ctxInfo) {
    test_composed_imagefilter_offset(reporter, ctxInfo.fGrContext);
}
#endif

static void test_composed_imagefilter_bounds(skiatest::Reporter* reporter, GrContext* context) {
    // The bounds passed to the inner filter must be filtered by the outer
    // filter, so that the inner filter produces the pixels that the outer
    // filter requires as input. This matters if the outer filter moves pixels.
    // Here, accounting for the outer offset is necessary so that the green
    // pixels of the picture are not clipped.

    SkPictureRecorder recorder;
    SkCanvas* recordingCanvas = recorder.beginRecording(SkRect::MakeWH(200, 100));
    recordingCanvas->clipRect(SkRect::MakeXYWH(100, 0, 100, 100));
    recordingCanvas->clear(SK_ColorGREEN);
    sk_sp<SkPicture> picture(recorder.finishRecordingAsPicture());
    sk_sp<SkImageFilter> pictureFilter(SkPictureImageFilter::Make(picture));
    SkImageFilter::CropRect cropRect(SkRect::MakeWH(100, 100));
    sk_sp<SkImageFilter> offsetFilter(SkOffsetImageFilter::Make(-100, 0, nullptr, &cropRect));
    sk_sp<SkImageFilter> composedFilter(SkComposeImageFilter::Make(std::move(offsetFilter),
                                                                   std::move(pictureFilter)));

    sk_sp<SkSpecialImage> sourceImage(create_empty_special_image(context, 100));
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(100, 100), nullptr);
    SkIPoint offset;
    sk_sp<SkSpecialImage> result(composedFilter->filterImage(sourceImage.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, offset.isZero());
    REPORTER_ASSERT(reporter, result);
    REPORTER_ASSERT(reporter, result->subset().size() == SkISize::Make(100, 100));

    SkBitmap resultBM;
    REPORTER_ASSERT(reporter, result->getROPixels(&resultBM));
    SkAutoLockPixels lock(resultBM);
    REPORTER_ASSERT(reporter, resultBM.getColor(50, 50) == SK_ColorGREEN);
}

DEF_TEST(ComposedImageFilterBounds, reporter) {
    test_composed_imagefilter_bounds(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(ComposedImageFilterBounds_Gpu, reporter, ctxInfo) {
    test_composed_imagefilter_bounds(reporter, ctxInfo.fGrContext);
}
#endif

static void test_partial_crop_rect(skiatest::Reporter* reporter, GrContext* context) {
    sk_sp<SkSpecialImage> srcImg(create_empty_special_image(context, 100));

    SkImageFilter::CropRect cropRect(SkRect::MakeXYWH(100, 0, 20, 30),
        SkImageFilter::CropRect::kHasWidth_CropEdge | SkImageFilter::CropRect::kHasHeight_CropEdge);
    sk_sp<SkImageFilter> filter(make_grayscale(nullptr, &cropRect));
    SkIPoint offset;
    SkImageFilter::Context ctx(SkMatrix::I(), SkIRect::MakeWH(100, 100), nullptr);

    sk_sp<SkSpecialImage> resultImg(filter->filterImage(srcImg.get(), ctx, &offset));
    REPORTER_ASSERT(reporter, resultImg);

    REPORTER_ASSERT(reporter, offset.fX == 0);
    REPORTER_ASSERT(reporter, offset.fY == 0);
    REPORTER_ASSERT(reporter, resultImg->width() == 20);
    REPORTER_ASSERT(reporter, resultImg->height() == 30);
}

DEF_TEST(ImageFilterPartialCropRect, reporter) {
    test_partial_crop_rect(reporter, nullptr);
}

#if SK_SUPPORT_GPU
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterPartialCropRect_Gpu, reporter, ctxInfo) {
    test_partial_crop_rect(reporter, ctxInfo.fGrContext);
}
#endif

DEF_TEST(ImageFilterCanComputeFastBounds, reporter) {

    {
        SkPoint3 location = SkPoint3::Make(0, 0, SK_Scalar1);
        sk_sp<SkImageFilter> lighting(SkLightingImageFilter::MakePointLitDiffuse(location,
                                                                                 SK_ColorGREEN,
                                                                                 0, 0, nullptr));
        REPORTER_ASSERT(reporter, !lighting->canComputeFastBounds());
    }

    {
        sk_sp<SkImageFilter> gray(make_grayscale(nullptr, nullptr));
        REPORTER_ASSERT(reporter, gray->canComputeFastBounds());
        {
            SkColorFilter* grayCF;
            REPORTER_ASSERT(reporter, gray->asAColorFilter(&grayCF));
            REPORTER_ASSERT(reporter, !grayCF->affectsTransparentBlack());
            grayCF->unref();
        }
        REPORTER_ASSERT(reporter, gray->canComputeFastBounds());

        sk_sp<SkImageFilter> grayBlur(SkBlurImageFilter::Make(SK_Scalar1, SK_Scalar1,
                                                              std::move(gray)));
        REPORTER_ASSERT(reporter, grayBlur->canComputeFastBounds());
    }

    {
        SkScalar greenMatrix[20] = { 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 1,
                                     0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 1 };
        sk_sp<SkColorFilter> greenCF(SkColorFilter::MakeMatrixFilterRowMajor255(greenMatrix));
        sk_sp<SkImageFilter> green(SkColorFilterImageFilter::Make(greenCF, nullptr));

        REPORTER_ASSERT(reporter, greenCF->affectsTransparentBlack());
        REPORTER_ASSERT(reporter, !green->canComputeFastBounds());

        sk_sp<SkImageFilter> greenBlur(SkBlurImageFilter::Make(SK_Scalar1, SK_Scalar1,
                                                               std::move(green)));
        REPORTER_ASSERT(reporter, !greenBlur->canComputeFastBounds());
    }

    uint8_t allOne[256], identity[256];
    for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        allOne[i] = 255;
    }

    sk_sp<SkColorFilter> identityCF(SkTableColorFilter::MakeARGB(identity, identity,
                                                                 identity, allOne));
    sk_sp<SkImageFilter> identityFilter(SkColorFilterImageFilter::Make(identityCF, nullptr));
    REPORTER_ASSERT(reporter, !identityCF->affectsTransparentBlack());
    REPORTER_ASSERT(reporter, identityFilter->canComputeFastBounds());

    sk_sp<SkColorFilter> forceOpaqueCF(SkTableColorFilter::MakeARGB(allOne, identity,
                                                                    identity, identity));
    sk_sp<SkImageFilter> forceOpaque(SkColorFilterImageFilter::Make(forceOpaqueCF, nullptr));
    REPORTER_ASSERT(reporter, forceOpaqueCF->affectsTransparentBlack());
    REPORTER_ASSERT(reporter, !forceOpaque->canComputeFastBounds());
}

// Verify that SkImageSource survives serialization
DEF_TEST(ImageFilterImageSourceSerialization, reporter) {
    auto surface(SkSurface::MakeRasterN32Premul(10, 10));
    surface->getCanvas()->clear(SK_ColorGREEN);
    sk_sp<SkImage> image(surface->makeImageSnapshot());
    sk_sp<SkImageFilter> filter(SkImageSource::Make(std::move(image)));

    sk_sp<SkData> data(SkValidatingSerializeFlattenable(filter.get()));
    sk_sp<SkFlattenable> flattenable(SkValidatingDeserializeFlattenable(
        data->data(), data->size(), SkImageFilter::GetFlattenableType()));
    SkImageFilter* unflattenedFilter = static_cast<SkImageFilter*>(flattenable.get());
    REPORTER_ASSERT(reporter, unflattenedFilter);

    SkBitmap bm;
    bm.allocN32Pixels(10, 10);
    bm.eraseColor(SK_ColorBLUE);
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setImageFilter(unflattenedFilter);

    SkCanvas canvas(bm);
    canvas.drawRect(SkRect::MakeWH(10, 10), paint);
    REPORTER_ASSERT(reporter, *bm.getAddr32(0, 0) == SkPreMultiplyColor(SK_ColorGREEN));
}

static void test_large_blur_input(skiatest::Reporter* reporter, SkCanvas* canvas) {
    SkBitmap largeBmp;
    int largeW = 5000;
    int largeH = 5000;
#if SK_SUPPORT_GPU
    // If we're GPU-backed make the bitmap too large to be converted into a texture.
    if (GrContext* ctx = canvas->getGrContext()) {
        largeW = ctx->caps()->maxTextureSize() + 1;
    }
#endif

    largeBmp.allocN32Pixels(largeW, largeH);
    largeBmp.eraseColor(0);
    if (!largeBmp.getPixels()) {
        ERRORF(reporter, "Failed to allocate large bmp.");
        return;
    }

    sk_sp<SkImage> largeImage(SkImage::MakeFromBitmap(largeBmp));
    if (!largeImage) {
        ERRORF(reporter, "Failed to create large image.");
        return;
    }

    sk_sp<SkImageFilter> largeSource(SkImageSource::Make(std::move(largeImage)));
    if (!largeSource) {
        ERRORF(reporter, "Failed to create large SkImageSource.");
        return;
    }

    sk_sp<SkImageFilter> blur(SkBlurImageFilter::Make(10.f, 10.f, std::move(largeSource)));
    if (!blur) {
        ERRORF(reporter, "Failed to create SkBlurImageFilter.");
        return;
    }

    SkPaint paint;
    paint.setImageFilter(std::move(blur));

    // This should not crash (http://crbug.com/570479).
    canvas->drawRect(SkRect::MakeIWH(largeW, largeH), paint);
}

DEF_TEST(ImageFilterBlurLargeImage, reporter) {
    auto surface(SkSurface::MakeRaster(SkImageInfo::MakeN32Premul(100, 100)));
    test_large_blur_input(reporter, surface->getCanvas());
}

#if SK_SUPPORT_GPU

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ImageFilterHugeBlur_Gpu, reporter, ctxInfo) {

    sk_sp<SkSurface> surf(SkSurface::MakeRenderTarget(ctxInfo.fGrContext,
                                                      SkBudgeted::kNo,
                                                      SkImageInfo::MakeN32Premul(100, 100)));


    SkCanvas* canvas = surf->getCanvas();

    test_huge_blur(canvas, reporter);
}

DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(XfermodeImageFilterCroppedInput_Gpu, reporter, ctxInfo) {

    sk_sp<SkSurface> surf(SkSurface::MakeRenderTarget(ctxInfo.fGrContext,
                                                      SkBudgeted::kNo,
                                                      SkImageInfo::MakeN32Premul(1, 1)));


    SkCanvas* canvas = surf->getCanvas();

    test_xfermode_cropped_input(canvas, reporter);
}

DEF_GPUTEST_FOR_ALL_GL_CONTEXTS(ImageFilterBlurLargeImage_Gpu, reporter, ctxInfo) {
    auto surface(SkSurface::MakeRenderTarget(ctxInfo.fGrContext, SkBudgeted::kYes,
                                             SkImageInfo::MakeN32Premul(100, 100)));
    test_large_blur_input(reporter, surface->getCanvas());
}
#endif

/*
 *  Test that colorfilterimagefilter does not require its CTM to be decomposed when it has more
 *  than just scale/translate, but that other filters do.
 */
DEF_TEST(ImageFilterDecomposeCTM, reporter) {
    // just need a colorfilter to exercise the corresponding imagefilter
    sk_sp<SkColorFilter> cf = SkColorFilter::MakeModeFilter(SK_ColorRED, SkXfermode::kSrcATop_Mode);
    sk_sp<SkImageFilter> cfif = SkColorFilterImageFilter::Make(cf, nullptr);
    sk_sp<SkImageFilter> blif = SkBlurImageFilter::Make(3, 3, nullptr);

    struct {
        sk_sp<SkImageFilter> fFilter;
        bool                 fExpectCanHandle;
    } recs[] = {
        { cfif,                                     true  },
        { SkColorFilterImageFilter::Make(cf, cfif), true  },
        { SkMergeImageFilter::Make(cfif, cfif),     true  },
        { blif,                                     false },
        { SkMergeImageFilter::Make(cfif, blif),     false },
        { SkColorFilterImageFilter::Make(cf, blif), false },
    };
    
    for (const auto& rec : recs) {
        const bool canHandle = rec.fFilter->canHandleAffine();
        REPORTER_ASSERT(reporter, canHandle == rec.fExpectCanHandle);
    }
}
