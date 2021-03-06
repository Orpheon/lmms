/* Calf DSP Library
 * Reusable audio effect classes - implementation.
 *
 * Copyright (C) 2001-2010 Krzysztof Foltman, Markus Schmidt, Thor Harald Johansen and others
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include <calf/audio_fx.h>
#include <calf/giface.h>

using namespace calf_plugins;
using namespace dsp;

simple_phaser::simple_phaser(int _max_stages, float *x1vals, float *y1vals)
{
    max_stages = _max_stages;
    x1 = x1vals;
    y1 = y1vals;

    set_base_frq(1000);
    set_mod_depth(1000);
    set_fb(0);
    state = 0;
    cnt = 0;
    stages = 0;
    set_stages(_max_stages);    
}

void simple_phaser::set_stages(int _stages)
{
    if (_stages > stages)
    {
        assert(_stages <= max_stages);
        if (_stages > max_stages)
            _stages = max_stages;
        for (int i = stages; i < _stages; i++)
        {
            x1[i] = x1[stages-1];
            y1[i] = y1[stages-1];
        }
    }
    stages = _stages;
}

void simple_phaser::reset()
{
    cnt = 0;
    state = 0;
    phase.set(0);
    for (int i = 0; i < max_stages; i++)
        x1[i] = y1[i] = 0;
    control_step();
}

void simple_phaser::control_step()
{
    cnt = 0;
    int v = phase.get() + 0x40000000;
    int sign = v >> 31;
    v ^= sign;
    // triangle wave, range from 0 to INT_MAX
    double vf = (double)((v >> 16) * (1.0 / 16384.0) - 1);
    
    float freq = base_frq * pow(2.0, vf * mod_depth / 1200.0);
    freq = dsp::clip<float>(freq, 10.0, 0.49 * sample_rate);
    stage1.set_ap_w(freq * (M_PI / 2.0) * odsr);
    phase += dphase * 32;
    for (int i = 0; i < stages; i++)
    {
        dsp::sanitize(x1[i]);
        dsp::sanitize(y1[i]);
    }
    dsp::sanitize(state);
}

void simple_phaser::process(float *buf_out, float *buf_in, int nsamples)
{
    for (int i=0; i<nsamples; i++) {
        cnt++;
        if (cnt == 32)
            control_step();
        float in = *buf_in++;
        float fd = in + state * fb;
        for (int j = 0; j < stages; j++)
            fd = stage1.process_ap(fd, x1[j], y1[j]);
        state = fd;
        
        float sdry = in * gs_dry.get();
        float swet = fd * gs_wet.get();
        *buf_out++ = sdry + swet;
    }
}

float simple_phaser::freq_gain(float freq, float sr) const
{
    typedef std::complex<double> cfloat;
    freq *= 2.0 * M_PI / sr;
    cfloat z = 1.0 / exp(cfloat(0.0, freq)); // z^-1
    
    cfloat p = cfloat(1.0);
    cfloat stg = stage1.h_z(z);
    
    for (int i = 0; i < stages; i++)
        p = p * stg;
    
    p = p / (cfloat(1.0) - cfloat(fb) * p);        
    return std::abs(cfloat(gs_dry.get_last()) + cfloat(gs_wet.get_last()) * p);
}

///////////////////////////////////////////////////////////////////////////////////

void biquad_filter_module::calculate_filter(float freq, float q, int mode, float gain)
{
    if (mode <= mode_36db_lp) {
        order = mode + 1;
        left[0].set_lp_rbj(freq, pow(q, 1.0 / order), srate, gain);
    } else if ( mode_12db_hp <= mode && mode <= mode_36db_hp ) {
        order = mode - mode_12db_hp + 1;
        left[0].set_hp_rbj(freq, pow(q, 1.0 / order), srate, gain);
    } else if ( mode_6db_bp <= mode && mode <= mode_18db_bp ) {
        order = mode - mode_6db_bp + 1;
        left[0].set_bp_rbj(freq, pow(q, 1.0 / order), srate, gain);
    } else { // mode_6db_br <= mode <= mode_18db_br
        order = mode - mode_6db_br + 1;
        left[0].set_br_rbj(freq, order * 0.1 * q, srate, gain);
    }
    
    right[0].copy_coeffs(left[0]);
    for (int i = 1; i < order; i++) {
        left[i].copy_coeffs(left[0]);
        right[i].copy_coeffs(left[0]);
    }
}

void biquad_filter_module::filter_activate()
{
    for (int i=0; i < order; i++) {
        left[i].reset();
        right[i].reset();
    }
}

void biquad_filter_module::sanitize()
{
    for (int i=0; i < order; i++) {
        left[i].sanitize();
        right[i].sanitize();
    }
}

int biquad_filter_module::process_channel(uint16_t channel_no, const float *in, float *out, uint32_t numsamples, int inmask) {
    dsp::biquad_d1<float> *filter;
    switch (channel_no) {
    case 0:
        filter = left;
        break;
        
    case 1:
        filter = right;
        break;
    
    default:
        assert(false);
        return 0;
    }
    
    if (inmask) {
        switch(order) {
            case 1:
                for (uint32_t i = 0; i < numsamples; i++)
                    out[i] = filter[0].process(in[i]);
                break;
            case 2:
                for (uint32_t i = 0; i < numsamples; i++)
                    out[i] = filter[1].process(filter[0].process(in[i]));
                break;
            case 3:
                for (uint32_t i = 0; i < numsamples; i++)
                    out[i] = filter[2].process(filter[1].process(filter[0].process(in[i])));
                break;
        }
    } else {
        if (filter[order - 1].empty())
            return 0;
        switch(order) {
            case 1:
                for (uint32_t i = 0; i < numsamples; i++)
                    out[i] = filter[0].process_zeroin();
                break;
            case 2:
                if (filter[0].empty())
                    for (uint32_t i = 0; i < numsamples; i++)
                        out[i] = filter[1].process_zeroin();
                else
                    for (uint32_t i = 0; i < numsamples; i++)
                        out[i] = filter[1].process(filter[0].process_zeroin());
                break;
            case 3:
                if (filter[1].empty())
                    for (uint32_t i = 0; i < numsamples; i++)
                        out[i] = filter[2].process_zeroin();
                else
                    for (uint32_t i = 0; i < numsamples; i++)
                        out[i] = filter[2].process(filter[1].process(filter[0].process_zeroin()));
                break;
        }
    }
    for (int i = 0; i < order; i++)
        filter[i].sanitize();
    return filter[order - 1].empty() ? 0 : inmask;
}

float biquad_filter_module::freq_gain(int subindex, float freq, float srate) const
{
    float level = 1.0;
    for (int j = 0; j < order; j++)
        level *= left[j].freq_gain(freq, srate);
    return level;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void reverb::update_times()
{
    switch(type)
    {
    case 0:
        tl[0] =  397 << 16, tr[0] =  383 << 16;
        tl[1] =  457 << 16, tr[1] =  429 << 16;
        tl[2] =  549 << 16, tr[2] =  631 << 16;
        tl[3] =  649 << 16, tr[3] =  756 << 16;
        tl[4] =  773 << 16, tr[4] =  803 << 16;
        tl[5] =  877 << 16, tr[5] =  901 << 16;
        break;
    case 1:
        tl[0] =  697 << 16, tr[0] =  783 << 16;
        tl[1] =  957 << 16, tr[1] =  929 << 16;
        tl[2] =  649 << 16, tr[2] =  531 << 16;
        tl[3] = 1049 << 16, tr[3] = 1177 << 16;
        tl[4] =  473 << 16, tr[4] =  501 << 16;
        tl[5] =  587 << 16, tr[5] =  681 << 16;
        break;
    case 2:
    default:
        tl[0] =  697 << 16, tr[0] =  783 << 16;
        tl[1] =  957 << 16, tr[1] =  929 << 16;
        tl[2] =  649 << 16, tr[2] =  531 << 16;
        tl[3] = 1249 << 16, tr[3] = 1377 << 16;
        tl[4] = 1573 << 16, tr[4] = 1671 << 16;
        tl[5] = 1877 << 16, tr[5] = 1781 << 16;
        break;
    case 3:
        tl[0] = 1097 << 16, tr[0] = 1087 << 16;
        tl[1] = 1057 << 16, tr[1] = 1031 << 16;
        tl[2] = 1049 << 16, tr[2] = 1039 << 16;
        tl[3] = 1083 << 16, tr[3] = 1055 << 16;
        tl[4] = 1075 << 16, tr[4] = 1099 << 16;
        tl[5] = 1003 << 16, tr[5] = 1073 << 16;
        break;
    case 4:
        tl[0] =  197 << 16, tr[0] =  133 << 16;
        tl[1] =  357 << 16, tr[1] =  229 << 16;
        tl[2] =  549 << 16, tr[2] =  431 << 16;
        tl[3] =  949 << 16, tr[3] = 1277 << 16;
        tl[4] = 1173 << 16, tr[4] = 1671 << 16;
        tl[5] = 1477 << 16, tr[5] = 1881 << 16;
        break;
    case 5:
        tl[0] =  197 << 16, tr[0] =  133 << 16;
        tl[1] =  257 << 16, tr[1] =  179 << 16;
        tl[2] =  549 << 16, tr[2] =  431 << 16;
        tl[3] =  619 << 16, tr[3] =  497 << 16;
        tl[4] = 1173 << 16, tr[4] = 1371 << 16;
        tl[5] = 1577 << 16, tr[5] = 1881 << 16;
        break;
    }
    
    float fDec=1000 + 2400.f * diffusion;
    for (int i = 0 ; i < 6; i++) {
        ldec[i]=exp(-float(tl[i] >> 16) / fDec), 
        rdec[i]=exp(-float(tr[i] >> 16) / fDec);
    }
}

void reverb::reset()
{
    apL1.reset();apR1.reset();
    apL2.reset();apR2.reset();
    apL3.reset();apR3.reset();
    apL4.reset();apR4.reset();
    apL5.reset();apR5.reset();
    apL6.reset();apR6.reset();
    lp_left.reset();lp_right.reset();
    old_left = 0; old_right = 0;
}

void reverb::process(float &left, float &right)
{
    unsigned int ipart = phase.ipart();
    
    // the interpolated LFO might be an overkill here
    int lfo = phase.lerp_by_fract_int<int, 14, int>(sine.data[ipart], sine.data[ipart+1]) >> 2;
    phase += dphase;
    
    left += old_right;
    left = apL1.process_allpass_comb_lerp16(left, tl[0] - 45*lfo, ldec[0]);
    left = apL2.process_allpass_comb_lerp16(left, tl[1] + 47*lfo, ldec[1]);
    float out_left = left;
    left = apL3.process_allpass_comb_lerp16(left, tl[2] + 54*lfo, ldec[2]);
    left = apL4.process_allpass_comb_lerp16(left, tl[3] - 69*lfo, ldec[3]);
    left = apL5.process_allpass_comb_lerp16(left, tl[4] + 69*lfo, ldec[4]);
    left = apL6.process_allpass_comb_lerp16(left, tl[5] - 46*lfo, ldec[5]);
    old_left = lp_left.process(left * fb);
    sanitize(old_left);

    right += old_left;
    right = apR1.process_allpass_comb_lerp16(right, tr[0] - 45*lfo, rdec[0]);
    right = apR2.process_allpass_comb_lerp16(right, tr[1] + 47*lfo, rdec[1]);
    float out_right = right;
    right = apR3.process_allpass_comb_lerp16(right, tr[2] + 54*lfo, rdec[2]);
    right = apR4.process_allpass_comb_lerp16(right, tr[3] - 69*lfo, rdec[3]);
    right = apR5.process_allpass_comb_lerp16(right, tr[4] + 69*lfo, rdec[4]);
    right = apR6.process_allpass_comb_lerp16(right, tr[5] - 46*lfo, rdec[5]);
    old_right = lp_right.process(right * fb);
    sanitize(old_right);
    
    left = out_left, right = out_right;
}

/// Distortion Module by Tom Szilagyi
///
/// This module provides a blendable saturation stage
///////////////////////////////////////////////////////////////////////////////////////////////

tap_distortion::tap_distortion()
{
    is_active = false;
    srate = 0;
    meter = 0.f;
}

void tap_distortion::activate()
{
    is_active = true;
    set_params(0.f, 0.f);
}
void tap_distortion::deactivate()
{
    is_active = false;
}

void tap_distortion::set_params(float blend, float drive)
{
    // set distortion coeffs
    if ((drive_old != drive) || (blend_old != blend)) {
        rdrive = 12.0f / drive;
        rbdr = rdrive / (10.5f - blend) * 780.0f / 33.0f;
        kpa = D(2.0f * (rdrive*rdrive) - 1.0f) + 1.0f;
        kpb = (2.0f - kpa) / 2.0f;
        ap = ((rdrive*rdrive) - kpa + 1.0f) / 2.0f;
        kc = kpa / D(2.0f * D(2.0f * (rdrive*rdrive) - 1.0f) - 2.0f * rdrive*rdrive);

        srct = (0.1f * srate) / (0.1f * srate + 1.0f);
        sq = kc*kc + 1.0f;
        knb = -1.0f * rbdr / D(sq);
        kna = 2.0f * kc * rbdr / D(sq);
        an = rbdr*rbdr / sq;
        imr = 2.0f * knb + D(2.0f * kna + 4.0f * an - 1.0f);
        pwrq = 2.0f / (imr + 1.0f);
        
        drive_old = drive;
        blend_old = blend;
    }
}

void tap_distortion::set_sample_rate(uint32_t sr)
{
    srate = sr;
}

float tap_distortion::process(float in)
{
    meter = 0.f;
    float out = 0.f;
    float proc = in;
    float med;
    if (proc >= 0.0f) {
        med = (D(ap + proc * (kpa - proc)) + kpb) * pwrq;
    } else {
        med = (D(an - proc * (kna + proc)) + knb) * pwrq * -1.0f;
    }
    proc = srct * (med - prev_med + prev_out);
    prev_med = M(med);
    prev_out = M(proc);
    out = proc;
    meter = proc;
    return out;
}

float tap_distortion::get_distortion_level()
{
    return meter;
}

////////////////////////////////////////////////////////////////////////////////

simple_lfo::simple_lfo()
{
    is_active       = false;
    phase = 0.f;
}

void simple_lfo::activate()
{
    is_active = true;
    phase = 0.f;
}

void simple_lfo::deactivate()
{
    is_active = false;
}

float simple_lfo::get_value()
{
    return get_value_from_phase(phase, offset) * amount;
}

float simple_lfo::get_value_from_phase(float ph, float off) const
{
    float val = 0.f;
    float phs = ph + off;
    if (phs >= 1.0)
        phs = fmod(phs, 1.f);
    switch (mode) {
        default:
        case 0:
            // sine
            val = sin((phs * 360.f) * M_PI / 180);
            break;
        case 1:
            // triangle
            if(phs > 0.75)
                val = (phs - 0.75) * 4 - 1;
            else if(phs > 0.5)
                val = (phs - 0.5) * 4 * -1;
            else if(phs > 0.25)
                val = 1 - (phs - 0.25) * 4;
            else
                val = phs * 4;
            break;
        case 2:
            // square
            val = (phs < 0.5) ? -1 : +1;
            break;
        case 3:
            // saw up
                val = phs * 2.f - 1;
            break;
        case 4:
            // saw down
            val = 1 - phs * 2.f;
            break;
    }
    return val;
}

void simple_lfo::advance(uint32_t count)
{
    //this function walks from 0.f to 1.f and starts all over again
    phase += count * freq * (1.0 / srate);
    if (phase >= 1.0)
        phase = fmod(phase, 1.f);
}

void simple_lfo::set_phase(float ph)
{
    //set the phase from outsinde
    phase = fabs(ph);
    if (phase >= 1.0)
        phase = fmod(phase, 1.f);
}

void simple_lfo::set_params(float f, int m, float o, uint32_t sr, float a)
{
    // freq: a value in Hz
    // mode: sine=0, triangle=1, square=2, saw_up=3, saw_down=4
    // offset: value between 0.f and 1.f to offset the lfo in time
    freq = f;
    mode = m;
    offset = o;
    srate = sr;
    amount = a;
}

bool simple_lfo::get_graph(float *data, int points, cairo_iface *context) const
{
    if (!is_active)
        return false;
    for (int i = 0; i < points; i++) {
        float ph = (float)i / (float)points;
        data[i] = get_value_from_phase(ph, offset) * amount;
    }
    return true;
}

bool simple_lfo::get_dot(float &x, float &y, int &size, cairo_iface *context) const
{
    if (!is_active)
        return false;
    float phs = phase + offset;
    if (phs >= 1.0)
        phs = fmod(phs, 1.f);
    x = phase;
    y = get_value_from_phase(phase, offset) * amount;
    return true;
}

