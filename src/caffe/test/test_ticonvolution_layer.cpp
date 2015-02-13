// Angjoo Kanazawa 2013

#include <cstring>
#include <cuda_runtime.h>

#include "gtest/gtest.h"
#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/vision_layers.hpp"

#include "caffe/test/test_caffe_main.hpp"
#include "caffe/test/test_gradient_check_util.hpp"

namespace caffe {

template <typename TypeParam>
class TIConvolutionLayerTest : public MultiDeviceTest<TypeParam> {
  typedef typename TypeParam::Dtype Dtype;

protected:
  TIConvolutionLayerTest()
      : blob_bottom_(new Blob<Dtype>()), blob_top_(new Blob<Dtype>()) {};
  virtual void SetUp() {
    // Caffe::set_random_seed(1701);
    blob_bottom_->Reshape(2, 2, 7, 7);
    // Fill the values
    FillerParameter filler_param;
    filler_param.set_value(1.);
    // Filling this with a constant will make the numerical gradient test fail,
    // bc small changes count a lot bc of uplayer.
    GaussianFiller<Dtype> filler(filler_param);
    filler.Fill(this->blob_bottom_);
    blob_bottom_vec_.push_back(blob_bottom_);
    blob_top_vec_.push_back(blob_top_);
  };

  virtual ~TIConvolutionLayerTest() {
    delete blob_bottom_;
    delete blob_top_;
  }
  Blob<Dtype> *const blob_bottom_;
  Blob<Dtype> *const blob_top_;
  vector<Blob<Dtype> *> blob_bottom_vec_;
  vector<Blob<Dtype> *> blob_top_vec_;

  void printMat(const float *data, const int &row, const int &col) {
    for (int i = 0; i < row * col; ++i) {
      printf("%.3f\t", data[i]);
      if ((i + 1) % col == 0)
        printf("\n");
    }
    printf("\n");
  }
};

TYPED_TEST_CASE(TIConvolutionLayerTest, TestDtypesAndDevices);

TYPED_TEST(TIConvolutionLayerTest, TestSetup) {
  typedef typename TypeParam::Dtype Dtype;

  this->blob_bottom_->Reshape(2, 3, 12, 10);

  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();
  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(2);
  convolution_param->set_num_output(4);

  // AJ: TI part:
  layer_param.add_transformations(); // add identity
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(1.15);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_scale(0.5);

  shared_ptr<Layer<Dtype> > layer(new TIConvolutionLayer<Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  EXPECT_EQ(this->blob_top_->num(), 2);
  EXPECT_EQ(this->blob_top_->channels(), 4);
  EXPECT_EQ(this->blob_top_->height(), 5);
  EXPECT_EQ(this->blob_top_->width(), 4);

  // Setting group should not change the shape
  convolution_param->set_num_output(3);
  convolution_param->set_group(3);
  layer.reset(new TIConvolutionLayer<Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  EXPECT_EQ(this->blob_top_->num(), 2);
  EXPECT_EQ(this->blob_top_->channels(), 3);
  EXPECT_EQ(this->blob_top_->height(), 5);
  EXPECT_EQ(this->blob_top_->width(), 4);
}

TYPED_TEST(TIConvolutionLayerTest, TestSimpleTIConvolution) {
  typedef typename TypeParam::Dtype Dtype;
  // We will simply see if the convolution layer carries out averaging well.
  this->blob_bottom_->Reshape(2, 3, 6, 5);

  FillerParameter filler_param;
  filler_param.set_value(1.);
  ConstantFiller<Dtype> filler(filler_param);
  filler.Fill(this->blob_bottom_);
  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();
  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(2);
  convolution_param->set_num_output(4);
  convolution_param->mutable_weight_filler()->set_type("constant");
  convolution_param->mutable_weight_filler()->set_value(1);
  convolution_param->mutable_bias_filler()->set_type("constant");
  convolution_param->mutable_bias_filler()->set_value(0.1);

  // AJ: TI part:
  layer_param.add_transformations(); // add identity
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(1.15);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_rotation(5);

  shared_ptr<Layer<Dtype> > layer(new TIConvolutionLayer<Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  Caffe::set_mode(Caffe::CPU);
  layer->Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  // After the convolution, the output should all have output values 27.1
  const Dtype *top_data = this->blob_top_->cpu_data();
  for (int i = 0; i < this->blob_top_->count(); ++i) {
    EXPECT_GE(top_data[i], 27.1 - 1e-4);
    EXPECT_LE(top_data[i], 27.1 + 1e-4);
  }
  // Test GPU
  Caffe::set_mode(Caffe::GPU);
  layer->Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  // After the convolution, the output should all have output values 27.1
  top_data = this->blob_top_->cpu_data();
  for (int i = 0; i < this->blob_top_->count(); ++i) {
    EXPECT_GE(top_data[i], 27.1 - 1e-4);
    EXPECT_LE(top_data[i], 27.1 + 1e-4);
  }
}

TYPED_TEST(TIConvolutionLayerTest, TestSimpleTIConvolution_bilinear) {
  typedef typename TypeParam::Dtype Dtype;
  // We will simply see if the convolution layer carries out averaging well.
  this->blob_bottom_->Reshape(2, 3, 6, 5);

  FillerParameter filler_param;
  filler_param.set_value(1.);
  ConstantFiller<Dtype> filler(filler_param);
  filler.Fill(this->blob_bottom_);
  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();
  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(2);
  convolution_param->set_num_output(4);
  convolution_param->mutable_weight_filler()->set_type("constant");
  convolution_param->mutable_weight_filler()->set_value(1);
  convolution_param->mutable_bias_filler()->set_type("constant");
  convolution_param->mutable_bias_filler()->set_value(0.1);

  // AJ: TI part:
  TransParameter *t0 = layer_param.add_transformations(); // add identity
  t0->set_interp(BILINEAR);
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(1.15);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_rotation(5);

  shared_ptr<Layer<Dtype> > layer(new TIConvolutionLayer<Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  Caffe::set_mode(Caffe::CPU);
  layer->Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  // After the convolution, the output should all have output values 27.1
  const Dtype *top_data = this->blob_top_->cpu_data();
  for (int i = 0; i < this->blob_top_->count(); ++i) {
    EXPECT_GE(top_data[i], 27.1 - 1e-4);
    EXPECT_LE(top_data[i], 27.1 + 1e-4);
  }
  // Test GPU
  Caffe::set_mode(Caffe::GPU);
  layer->Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  // After the convolution, the output should all have output values 27.1
  top_data = this->blob_top_->cpu_data();
  for (int i = 0; i < this->blob_top_->count(); ++i) {
    EXPECT_GE(top_data[i], 27.1 - 1e-4);
    EXPECT_LE(top_data[i], 27.1 + 1e-4);
  }
}

TYPED_TEST(TIConvolutionLayerTest, TestSimpleTIConvolutionGroup) {
  typedef typename TypeParam::Dtype Dtype;
  // We will simply see if the convolution layer carries out averaging well.
  this->blob_bottom_->Reshape(2, 3, 6, 5);

  FillerParameter filler_param;
  filler_param.set_value(1.);
  ConstantFiller<Dtype> filler(filler_param);
  filler.Fill(this->blob_bottom_);
  Dtype *bottom_data = this->blob_bottom_->mutable_cpu_data();
  for (int n = 0; n < this->blob_bottom_->num(); ++n) {
    for (int c = 0; c < this->blob_bottom_->channels(); ++c) {
      for (int h = 0; h < this->blob_bottom_->height(); ++h) {
        for (int w = 0; w < this->blob_bottom_->width(); ++w) {
          bottom_data[this->blob_bottom_->offset(n, c, h, w)] = c;
        }
      }
    }
  }
  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();
  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(2);
  convolution_param->set_num_output(3);
  convolution_param->set_group(3);
  convolution_param->mutable_weight_filler()->set_type("constant");
  convolution_param->mutable_weight_filler()->set_value(1);
  convolution_param->mutable_bias_filler()->set_type("constant");
  convolution_param->mutable_bias_filler()->set_value(0.1);

  // AJ: TI part:
  layer_param.add_transformations(); // add identity
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(1.15);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_rotation(5);

  shared_ptr<Layer<Dtype> > layer(new TIConvolutionLayer<Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, &(this->blob_top_vec_));
  Caffe::set_mode(Caffe::CPU);
  layer->Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  // After the convolution, the output should all have output values 9.1
  const Dtype *top_data = this->blob_top_->cpu_data();
  for (int n = 0; n < this->blob_top_->num(); ++n) {
    for (int c = 0; c < this->blob_top_->channels(); ++c) {
      for (int h = 0; h < this->blob_top_->height(); ++h) {
        for (int w = 0; w < this->blob_top_->width(); ++w) {
          Dtype data = top_data[this->blob_top_->offset(n, c, h, w)];
          EXPECT_GE(data, c * 9 + 0.1 - 1e-4);
          EXPECT_LE(data, c * 9 + 0.1 + 1e-4);
        }
      }
    }
  }
  // Test GPU
  Caffe::set_mode(Caffe::GPU);
  layer->Forward(this->blob_bottom_vec_, &(this->blob_top_vec_));
  // After the convolution, the output should all have output values 9.1
  top_data = this->blob_top_->cpu_data();
  for (int n = 0; n < this->blob_top_->num(); ++n) {
    for (int c = 0; c < this->blob_top_->channels(); ++c) {
      for (int h = 0; h < this->blob_top_->height(); ++h) {
        for (int w = 0; w < this->blob_top_->width(); ++w) {
          Dtype data = top_data[this->blob_top_->offset(n, c, h, w)];
          EXPECT_GE(data, c * 9 + 0.1 - 1e-4);
          EXPECT_LE(data, c * 9 + 0.1 + 1e-4);
        }
      }
    }
  }
}

TYPED_TEST(TIConvolutionLayerTest, TestGradient) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();
  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(1);
  convolution_param->set_num_output(3);
  convolution_param->mutable_weight_filler()->set_type("gaussian");
  convolution_param->mutable_bias_filler()->set_type("gaussian");

  // TI part:
  layer_param.add_transformations(); // add identity
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(2.);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_scale(.5);
  TransParameter *t3 = layer_param.add_transformations();
  t3->set_rotation(45);

  TIConvolutionLayer<Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-3, 1e-1);
  checker.CheckGradientExhaustive(&layer, &(this->blob_bottom_vec_),
                                  &(this->blob_top_vec_));
}

TYPED_TEST(TIConvolutionLayerTest, TestGradient_bilinear) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();

  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(1);
  convolution_param->set_num_output(3);
  convolution_param->mutable_weight_filler()->set_type("gaussian");
  convolution_param->mutable_bias_filler()->set_type("gaussian");

  // AJ: TI part:
  TransParameter *t0 = layer_param.add_transformations(); // add identity
  t0->set_interp(BILINEAR);
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(2.);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_scale(.5);
  TransParameter *t3 = layer_param.add_transformations();
  t3->set_rotation(45);

  TIConvolutionLayer<Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-3, 1e-2);
  checker.CheckGradientExhaustive(&layer, &(this->blob_bottom_vec_),
                                  &(this->blob_top_vec_));
}

TYPED_TEST(TIConvolutionLayerTest, TestGradientGroup) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  ConvolutionParameter *convolution_param =
      layer_param.mutable_convolution_param();
  convolution_param->set_kernel_size(3);
  convolution_param->set_stride(1);
  convolution_param->set_num_output(2);
  convolution_param->set_group(2);
  convolution_param->mutable_weight_filler()->set_type("gaussian");
  convolution_param->mutable_bias_filler()->set_type("gaussian");

  // AJ: TI part:
  layer_param.add_transformations(); // add identity
  TransParameter *t1 = layer_param.add_transformations();
  t1->set_scale(2.);
  TransParameter *t2 = layer_param.add_transformations();
  t2->set_scale(.5);
  TransParameter *t3 = layer_param.add_transformations();
  t3->set_rotation(45);

  TIConvolutionLayer<Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-4, 1e-2);
  checker.CheckGradientExhaustive(&layer, &(this->blob_bottom_vec_),
                                  &(this->blob_top_vec_));
}
}
