#ifndef TEXTNET_LAYER_GRU_LAYER_INL_HPP_
#define TEXTNET_LAYER_GRU_LAYER_INL_HPP_

#include <iostream>

#include <mshadow/tensor.h>
#include "../layer.h"
#include "../../utils/utils.h"
#include "../../io/json/json.h"
#include <cassert>

namespace textnet {
namespace layer {

template<typename xpu>
class GruLayer : public Layer<xpu> {
 public:
  GruLayer(LayerType type) { this->layer_type = type; }
  virtual ~GruLayer(void) {}
  
  virtual int BottomNodeNum() { return 1; }
  virtual int TopNodeNum() { return 1; }
  virtual int ParamNodeNum() { return 6; }

  typedef mshadow::Tensor<xpu, 1> Tensor1D;
  typedef mshadow::Tensor<xpu, 2> Tensor2D;
  typedef mshadow::Tensor<xpu, 3> Tensor3D;
  typedef mshadow::Tensor<xpu, 4> Tensor4D;
  
  virtual void Require() {
    // default value, just set the value you want
    // this->defaults["no_bias"] = SettingV(false);
    // this->defaults["no_out_tanh"] = SettingV(false);
    // this->defaults["param_file"] = SettingV("");
    // this->defaults["o_gate_bias_init"] = SettingV(0.f);
    // this->defaults["f_gate_bias_init"] = SettingV(0.f);
    // this->defaults["reverse"] = SettingV(false);
    
    // require value, set to SettingV(),
    // it will force custom to set in config
    // this->defaults["max_norm2"] = SettingV();
    // this->defaults["grad_norm2"] = SettingV();
    this->defaults["d_mem"] = SettingV();
    this->defaults["w_g_filler"] = SettingV();
    this->defaults["u_g_filler"] = SettingV();
    this->defaults["b_g_filler"] = SettingV();
    this->defaults["w_c_filler"] = SettingV();
    this->defaults["u_c_filler"] = SettingV();
    this->defaults["b_c_filler"] = SettingV();
    this->defaults["w_g_updater"] = SettingV();
    this->defaults["u_g_updater"] = SettingV();
    this->defaults["b_g_updater"] = SettingV();
    this->defaults["w_c_updater"] = SettingV();
    this->defaults["u_c_updater"] = SettingV();
    this->defaults["b_c_updater"] = SettingV();
    this->defaults["reverse"] = SettingV();
    // this->defaults["grad_cut_off"] = SettingV();
    
    Layer<xpu>::Require();
  }
  
  virtual void SetupLayer(std::map<std::string, SettingV> &setting,
                          const std::vector<Node<xpu>*> &bottom,
                          const std::vector<Node<xpu>*> &top,
                          mshadow::Random<xpu> *prnd) {
    Layer<xpu>::SetupLayer(setting, bottom, top, prnd);                        
    
    utils::Check(bottom.size() == BottomNodeNum(), "GruLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(), "GruLayer:top size problem.");
    utils::Check(setting.count("d_mem"), "GruLayer:setting problem.");
                  
    d_mem   = setting["d_mem"].iVal();
    d_input = bottom[0]->data.size(3);
    // no_bias = setting["no_bias"].bVal();
    // no_out_tanh = setting["no_out_tanh"].bVal();
    reverse = setting["reverse"].bVal();
    // grad_norm2 = setting["grad_norm2"].fVal();
    // param_file = setting["param_file"].sVal();
    // o_gate_bias_init = setting["o_gate_bias_init"].fVal();
    // f_gate_bias_init = setting["f_gate_bias_init"].fVal();
    // grad_cut_off = setting["grad_cut_off"].fVal();
    // max_norm2 = setting["max_norm2"].fVal();

    begin_h.Resize(mshadow::Shape2(1, d_mem), 0.f);
    // begin_c.Resize(mshadow::Shape2(1, d_mem), 0.f);
    begin_h_er.Resize(mshadow::Shape2(1, d_mem), 0.f);
    // begin_c_er.Resize(mshadow::Shape2(1, d_mem), 0.f);

    this->params.resize(6);
    this->params[0].Resize(1, 1, d_input, 2*d_mem, true); // w
    this->params[1].Resize(1, 1, d_mem,   2*d_mem, true); // u
    this->params[2].Resize(1, 1, 1,       2*d_mem, true); // b
    this->params[3].Resize(1, 1, d_input, 1*d_mem, true); // w
    this->params[4].Resize(1, 1, d_mem,   1*d_mem, true); // u
    this->params[5].Resize(1, 1, 1,       1*d_mem, true); // b
    
    std::map<std::string, SettingV> &w_g_setting = *setting["w_g_filler"].mVal();
    std::map<std::string, SettingV> &u_g_setting = *setting["u_g_filler"].mVal();
    std::map<std::string, SettingV> &b_g_setting = *setting["b_g_filler"].mVal();
    std::map<std::string, SettingV> &w_c_setting = *setting["w_c_filler"].mVal();
    std::map<std::string, SettingV> &u_c_setting = *setting["u_c_filler"].mVal();
    std::map<std::string, SettingV> &b_c_setting = *setting["b_c_filler"].mVal();
    this->params[0].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(w_g_setting["init_type"].iVal(), w_g_setting, this->prnd_);
    this->params[1].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(u_g_setting["init_type"].iVal(), u_g_setting, this->prnd_);
    this->params[2].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(b_g_setting["init_type"].iVal(), b_g_setting, this->prnd_);
    this->params[3].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(w_c_setting["init_type"].iVal(), w_c_setting, this->prnd_);
    this->params[4].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(u_c_setting["init_type"].iVal(), u_c_setting, this->prnd_);
    this->params[5].initializer_ = 
        initializer::CreateInitializer<xpu, 4>(b_c_setting["init_type"].iVal(), b_c_setting, this->prnd_);
    this->params[0].Init();
    this->params[1].Init();
    this->params[2].Init();
    this->params[3].Init();
    this->params[4].Init();
    this->params[5].Init();
    // if (f_gate_bias_init != 0.f) {
    //     init_f_gate_bias(); // this must be after init()
    // }
    // if (o_gate_bias_init != 0.f) {
    //     init_o_gate_bias(); // this must be after init()
    // }

    // if (!param_file.empty()) {
    //   LoadParam();
    // }
    
    std::map<std::string, SettingV> &w_g_updater = *setting["w_g_updater"].mVal();
    std::map<std::string, SettingV> &u_g_updater = *setting["u_g_updater"].mVal();
    std::map<std::string, SettingV> &b_g_updater = *setting["b_g_updater"].mVal();
    std::map<std::string, SettingV> &w_c_updater = *setting["w_c_updater"].mVal();
    std::map<std::string, SettingV> &u_c_updater = *setting["u_c_updater"].mVal();
    std::map<std::string, SettingV> &b_c_updater = *setting["b_c_updater"].mVal();

    this->params[0].updater_ = 
        updater::CreateUpdater<xpu, 4>(w_g_updater["updater_type"].iVal(), w_g_updater, this->prnd_);
    this->params[1].updater_ = 
        updater::CreateUpdater<xpu, 4>(u_g_updater["updater_type"].iVal(), u_g_updater, this->prnd_);
    this->params[2].updater_ = 
        updater::CreateUpdater<xpu, 4>(b_g_updater["updater_type"].iVal(), b_g_updater, this->prnd_);
    this->params[3].updater_ = 
        updater::CreateUpdater<xpu, 4>(w_c_updater["updater_type"].iVal(), w_c_updater, this->prnd_);
    this->params[4].updater_ = 
        updater::CreateUpdater<xpu, 4>(u_c_updater["updater_type"].iVal(), u_c_updater, this->prnd_);
    this->params[5].updater_ = 
        updater::CreateUpdater<xpu, 4>(b_c_updater["updater_type"].iVal(), b_c_updater, this->prnd_);
  }

  // if want to capture long term dependency, should init as a positive value
  // void init_f_gate_bias() {
  //   Tensor1D bias_data = this->params[2].data_d1();
  //   Tensor1D f_bias = Tensor1D(bias_data.dptr_ + 1*d_mem, mshadow::Shape1(d_mem));
  //   f_bias = f_gate_bias_init;
  // }

  // void init_o_gate_bias() {
  //   Tensor1D bias_data = this->params[2].data_d1();
  //   Tensor1D o_bias = Tensor1D(bias_data.dptr_ + 2*d_mem, mshadow::Shape1(d_mem));
  //   o_bias = o_gate_bias_init;
  // }
  
  // bottom should be padded with only one zero on both sides
  virtual void Reshape(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top,
					   bool show_info = false) {
    utils::Check(bottom.size() == BottomNodeNum(), "GruLayer:bottom size problem."); 
    utils::Check(top.size() == TopNodeNum(), "GruLayer:top size problem.");
    
    mshadow::Shape<4> shape_in  = bottom[0]->data.shape_;
    mshadow::Shape<4> shape_out = mshadow::Shape4(shape_in[0], shape_in[1], shape_in[2], d_mem);
    mshadow::Shape<4> shape_gate= mshadow::Shape4(shape_in[0], shape_in[1], shape_in[2], d_mem*2);

    top[0]->Resize(shape_out, true);
    c.Resize(shape_out, 0.f);
    g.Resize(shape_gate, 0.f);
    c_er.Resize(shape_out, 0.f);
    g_er.Resize(shape_gate, 0.f);

	if (show_info) {
	  bottom[0]->PrintShape("bottom0");
	  top[0]->PrintShape("top0");
    }
  }

  void checkNan(float *p, int l) {
      for (int i = 0; i < l; ++i) {
          assert(!isnan(p[i]));
      }
  }

  // void checkNanParams() {
  //     Tensor2D w_data = this->params[0].data[0][0];
  //     Tensor2D u_data = this->params[1].data[0][0];
  //     Tensor2D w_diff = this->params[0].diff[0][0];
  //     Tensor2D u_diff = this->params[1].diff[0][0];
  //     checkNan(w_data.dptr_, w_data.size(0) * w_data.size(1));
  //     checkNan(u_data.dptr_, u_data.size(0) * u_data.size(1));
  //     checkNan(w_diff.dptr_, w_diff.size(0) * w_diff.size(1));
  //     checkNan(u_diff.dptr_, u_diff.size(0) * u_diff.size(1));
  // }

  virtual void ForwardOneStep(Tensor2D pre_h,
                              Tensor2D x,
                              Tensor2D cur_g,
                              Tensor2D cur_c,
                              Tensor2D cur_h) {
      Tensor2D w_g_data = this->params[0].data[0][0];
      Tensor2D u_g_data = this->params[1].data[0][0];
      // Tensor2D b_g_data = this->params[2].data[0][0];
      Tensor2D w_c_data = this->params[3].data[0][0];
      Tensor2D u_c_data = this->params[4].data[0][0];
      // Tensor2D b_c_data = this->params[5].data[0][0];

      Tensor2D r, z;
      cur_g = dot(x, w_g_data);
      cur_g += dot(pre_h, u_g_data);
      // if (!no_bias) {
      //   cur_g += b_g_data;
      // }
      cur_g = mshadow::expr::F<op::sigmoid>(cur_g); // logi
      SplitGate(cur_g, r, z);

      cur_c = dot(x, w_c_data);
      mshadow::TensorContainer<xpu, 2> r_mul_pre_h(r.shape_);
      r_mul_pre_h = r * pre_h;
      cur_c += dot(r_mul_pre_h, u_c_data);
      // if (!no_bias) {
      //   cur_c += b_c_data;
      // }
      cur_c = mshadow::expr::F<op::tanh>(cur_c);

      cur_h = z*pre_h + (1-z)*cur_c;
  }

  void BpOneStep(Tensor2D cur_h_er,
                 Tensor2D pre_h,
                 Tensor2D x,
                 Tensor2D cur_g,
                 Tensor2D cur_c,
                 Tensor2D cur_h,
                 Tensor2D cur_c_er,
                 Tensor2D cur_g_er,
                 Tensor2D pre_h_er,
                 Tensor2D x_er) {
    Tensor2D w_g_data = this->params[0].data[0][0];
    Tensor2D u_g_data = this->params[1].data[0][0];
    Tensor2D w_c_data = this->params[3].data[0][0];
    Tensor2D u_c_data = this->params[4].data[0][0];
    Tensor2D w_g_er   = this->params[0].diff[0][0];
    Tensor2D u_g_er   = this->params[1].diff[0][0];
    // Tensor2D b_g_er   = this->params[2].diff[0][0];
    Tensor2D w_c_er   = this->params[3].diff[0][0];
    Tensor2D u_c_er   = this->params[4].diff[0][0];
    // Tensor2D b_c_er   = this->params[5].diff[0][0];

    Tensor2D r, z, r_er, z_er;
    SplitGate(cur_g, r, z);
    SplitGate(cur_g_er, r_er, z_er);

    pre_h_er += cur_h_er * z;
    cur_c_er += cur_h_er * (1-z);
    z_er += cur_h_er * pre_h; 
    z_er += cur_h_er * (-1. * cur_c);

    cur_c_er *= mshadow::expr::F<op::tanh_grad>(cur_c);
    x_er += dot(cur_c_er, w_c_data.T());
    w_c_er += dot(x.T(), cur_c_er);
    r_er = dot(cur_c_er, u_c_data.T());
    r_er *= pre_h;

    mshadow::TensorContainer<xpu, 2> tmp(pre_h_er.shape_);
    tmp = dot(cur_c_er, u_c_data.T());
    pre_h_er += r * tmp;

    mshadow::TensorContainer<xpu, 2> r_mul_pre_h(r.shape_);
    r_mul_pre_h = r * pre_h;
    u_c_er += dot((r_mul_pre_h).T(), cur_c_er);

    z_er *= mshadow::expr::F<op::sigmoid_grad>(z);
    r_er *= mshadow::expr::F<op::sigmoid_grad>(r);
    x_er += dot(cur_g_er, w_g_data.T());
    pre_h_er += dot(cur_g_er, u_g_data.T());
    w_g_er += dot(x.T(), cur_g_er);
    u_g_er += dot(pre_h.T(), cur_g_er);
  }

  void ForwardLeft2Right(Tensor2D in, Tensor2D g, Tensor2D c, Tensor2D out) {
      int begin = 0, end = in.size(0);
      Tensor2D pre_c, pre_h;
      // not need any padding, begin h and c are set to 0
      for (index_t row_idx = begin; row_idx < end; ++row_idx) {
        if (row_idx == begin) {
          // pre_c = begin_c; 
          pre_h = begin_h;
        } else {
          // pre_c = c.Slice(row_idx-1, row_idx);
          pre_h = out.Slice(row_idx-1, row_idx);
        }
        ForwardOneStep(pre_h,
                       in.Slice(row_idx, row_idx+1),
                       g.Slice(row_idx, row_idx+1), 
                       c.Slice(row_idx, row_idx+1), 
                       out.Slice(row_idx, row_idx+1));
      }
  }
  void ForwardRight2Left(Tensor2D in, Tensor2D g, Tensor2D c, Tensor2D out) {
      int begin = 0, end = in.size(0);
      Tensor2D pre_c, pre_h;
      // not need any padding, begin h and c are set to 0
      for (int row_idx = end-1; row_idx >= begin; --row_idx) {
        if (row_idx == end-1) {
          // pre_c = begin_c; 
          pre_h = begin_h;
        } else {
          // pre_c = c.Slice(row_idx+1, row_idx+2);
          pre_h = out.Slice(row_idx+1, row_idx+2);
        }
        ForwardOneStep(pre_h,
                       in.Slice(row_idx, row_idx+1),
                       g.Slice(row_idx, row_idx+1), 
                       c.Slice(row_idx, row_idx+1), 
                       out.Slice(row_idx, row_idx+1));
      }
  }
  
  virtual void Forward(const std::vector<Node<xpu>*> &bottom,
                       const std::vector<Node<xpu>*> &top) {
#if DEBUG
    // checkNanParams();
#endif
    Tensor4D bottom_data = bottom[0]->data;
    Tensor4D top_data = top[0]->data;
    top[0]->length = mshadow::expr::F<op::identity>(bottom[0]->length);
    top_data = 0.f; c = 0.f, g = 0.f; c_er = 0.f; g_er = 0.f;
    for (index_t batch_idx = 0; batch_idx < bottom_data.size(0); ++batch_idx) {
      for (index_t seq_idx = 0; seq_idx < bottom_data.size(1); ++seq_idx) {
        int len = bottom[0]->length[batch_idx][seq_idx];
        utils::Assert(len >= 0, "GruLayer: sequence length error.");
        if (!reverse) {
          ForwardLeft2Right(bottom_data[batch_idx][seq_idx].Slice(0,len), 
                            g[batch_idx][seq_idx].Slice(0,len), 
                            c[batch_idx][seq_idx].Slice(0,len), 
                            top_data[batch_idx][seq_idx].Slice(0,len));
        } else {
          ForwardRight2Left(bottom_data[batch_idx][seq_idx].Slice(0,len), 
                            g[batch_idx][seq_idx].Slice(0,len), 
                            c[batch_idx][seq_idx].Slice(0,len), 
                            top_data[batch_idx][seq_idx].Slice(0,len));
        }
      }
    }
#if DEBUG
    // checkNanParams();
#endif
  }

  // too tricky, may bring errors
  void SplitGate(Tensor2D g, Tensor2D &r, Tensor2D &z) {
    utils::Check(g.size(0) == 1, "GruLayer: gate problem."); 
    r = Tensor2D(g.dptr_, mshadow::Shape2(1, d_mem));
    z = Tensor2D(g.dptr_ + 1 * d_mem, mshadow::Shape2(1, d_mem));
  }

  // float norm2(Tensor2D t) {
  //   float norm2 = 0.f;
  //   for (int i = 0; i < t.size(0); ++i) {
  //     for (int j = 0; j < t.size(1); ++j) {
  //       norm2 += t[i][j] * t[i][j];
  //     }
  //   }
  //   if (norm2 == 0.f) norm2 = 0.0000000001;
  //   return sqrt(norm2);
  // }

  void BackpropForLeft2RightLstm(Tensor2D top_data, Tensor2D top_diff, 
                                 Tensor2D c, Tensor2D c_er, 
                                 Tensor2D g, Tensor2D g_er, 
                                 Tensor2D bottom_data, Tensor2D bottom_diff) {
      int begin = 0, end = top_data.size(0);

      Tensor2D pre_c, pre_h, pre_c_er, pre_h_er;
      for (int row_idx = end-1; row_idx >= begin; --row_idx) {
        if (row_idx == begin) {
            // pre_c = begin_c;
            pre_h = begin_h;
            // pre_c_er = begin_c_er;
            pre_h_er = begin_h_er;
        } else {
            // pre_c = c.Slice(row_idx-1, row_idx);
            pre_h = top_data.Slice(row_idx-1, row_idx);
            // pre_c_er = c_er.Slice(row_idx-1, row_idx);
            pre_h_er = top_diff.Slice(row_idx-1, row_idx);
        }
        BpOneStep(top_diff.Slice(row_idx, row_idx+1), 
                  pre_h,
                  bottom_data.Slice(row_idx, row_idx+1), 
                  g.Slice(row_idx, row_idx+1), 
                  c.Slice(row_idx, row_idx+1), 
                  top_data.Slice(row_idx, row_idx+1),
                  c_er.Slice(row_idx, row_idx+1),
                  g_er.Slice(row_idx, row_idx+1),
                  pre_h_er,
                  bottom_diff.Slice(row_idx, row_idx+1));
      }
  }
  void BackpropForRight2LeftLstm(Tensor2D top_data, Tensor2D top_diff, 
                                 Tensor2D c, Tensor2D c_er, 
                                 Tensor2D g, Tensor2D g_er, 
                                 Tensor2D bottom_data, Tensor2D bottom_diff) {
      int begin = 0, end = top_data.size(0);

      Tensor2D pre_c, pre_h, pre_c_er, pre_h_er;
      for (int row_idx = begin; row_idx < end; ++row_idx) {
        if (row_idx == end-1) {
            // pre_c = begin_c;
            pre_h = begin_h;
            // pre_c_er = begin_c_er;
            pre_h_er = begin_h_er;
        } else {
            // pre_c = c.Slice(row_idx+1, row_idx+2);
            pre_h = top_data.Slice(row_idx+1, row_idx+2);
            // pre_c_er = c_er.Slice(row_idx+1, row_idx+2);
            pre_h_er = top_diff.Slice(row_idx+1, row_idx+2);
        }
        BpOneStep(top_diff.Slice(row_idx, row_idx+1), 
                  pre_h,
                  bottom_data.Slice(row_idx, row_idx+1), 
                  g.Slice(row_idx, row_idx+1), 
                  c.Slice(row_idx, row_idx+1), 
                  top_data.Slice(row_idx, row_idx+1),
                  c_er.Slice(row_idx, row_idx+1),
                  g_er.Slice(row_idx, row_idx+1),
                  pre_h_er,
                  bottom_diff.Slice(row_idx, row_idx+1));
      }
  }

  
  virtual void Backprop(const std::vector<Node<xpu>*> &bottom,
                        const std::vector<Node<xpu>*> &top) {
    using namespace mshadow::expr;
#if DEBUG
    // checkNanParams();
#endif
    mshadow::Tensor<xpu, 4> top_diff = top[0]->diff;
    mshadow::Tensor<xpu, 4> top_data = top[0]->data;
    mshadow::Tensor<xpu, 4> bottom_data = bottom[0]->data;
    mshadow::Tensor<xpu, 4> bottom_diff = bottom[0]->diff;
        
    begin_h_er = 0.; g_er = 0.; c_er = 0.;
    for (index_t batch_idx = 0; batch_idx < bottom_data.size(0); ++batch_idx) {
      for (index_t seq_idx = 0; seq_idx < bottom_data.size(1); ++seq_idx) {
        int len = bottom[0]->length[batch_idx][seq_idx];
        utils::Assert(len >= 0, "GruLayer: sequence length error.");
        if (!reverse) {
            BackpropForLeft2RightLstm(top_data[batch_idx][seq_idx].Slice(0,len), 
                                      top_diff[batch_idx][seq_idx].Slice(0,len), 
                                      c[batch_idx][seq_idx].Slice(0,len), 
                                      c_er[batch_idx][seq_idx].Slice(0,len),
                                      g[batch_idx][seq_idx].Slice(0,len), 
                                      g_er[batch_idx][seq_idx].Slice(0,len), 
                                      bottom_data[batch_idx][seq_idx].Slice(0,len), 
                                      bottom_diff[batch_idx][seq_idx].Slice(0,len));
        } else {
            BackpropForRight2LeftLstm(top_data[batch_idx][seq_idx].Slice(0,len), 
                                      top_diff[batch_idx][seq_idx].Slice(0,len), 
                                      c[batch_idx][seq_idx].Slice(0,len), 
                                      c_er[batch_idx][seq_idx].Slice(0,len),
                                      g[batch_idx][seq_idx].Slice(0,len), 
                                      g_er[batch_idx][seq_idx].Slice(0,len), 
                                      bottom_data[batch_idx][seq_idx].Slice(0,len), 
                                      bottom_diff[batch_idx][seq_idx].Slice(0,len));
        }
      }
    }
    // this->params[0].CutOffGradient(grad_cut_off);
    // this->params[1].CutOffGradient(grad_cut_off);
    // this->params[2].CutOffGradient(grad_cut_off);

    // this->params[0].PrintStatistic("LSTM W");
    // this->params[1].PrintStatistic("LSTM U");
    // this->params[2].PrintStatistic("LSTM b");
#if DEBUG
    // checkNanParams();
#endif
  }
  // void LoadTensor(Json::Value &tensor_root, mshadow::TensorContainer<xpu, 4> &t) {
  //   Json::Value data_root = tensor_root["data"];
  //   int s0 = data_root["shape"][0].asInt();
  //   int s1 = data_root["shape"][1].asInt();
  //   int s2 = data_root["shape"][2].asInt();
  //   int s3 = data_root["shape"][3].asInt();
  //   utils::Check(t.size(0) == s0 && t.size(1) == s1 && t.size(2) == s2 && t.size(3) == s3, 
  //                "GruLayer: load tensor error.");
  //   int size = s0*s1*s2*s3;
  //   for (int i = 0; i < size; ++i) {
  //     t.dptr_[i] = data_root["value"][i].asFloat();
  //   }
  // }
  // void LoadParam() {
  //   utils::Printf("GruLayer: load params...\n");
  //   Json::Value param_root;
  //   ifstream ifs(param_file.c_str());
  //   ifs >> param_root;
  //   ifs.close();
  //   LoadTensor(param_root[0], this->params[0].data);
  //   LoadTensor(param_root[1], this->params[1].data);
  //   LoadTensor(param_root[2], this->params[2].data);
  // }

 public:
// protected:
  float max_norm2;
  int d_mem, d_input;
  bool no_bias, reverse, no_out_tanh; 
  float grad_norm2;
  float o_gate_bias_init;
  float f_gate_bias_init;
  float grad_cut_off;
  string param_file;
  mshadow::TensorContainer<xpu, 4> c, g, c_er, g_er;
  mshadow::TensorContainer<xpu, 2> begin_h, begin_h_er;
};
}  // namespace layer
}  // namespace textnet
#endif  
