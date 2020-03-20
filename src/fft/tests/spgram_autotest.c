/*
 * Copyright (c) 2007 - 2020 Joseph Gaeddert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// test spectral periodogram (spgram) objects

#include <stdlib.h>
#include "autotest/autotest.h"
#include "liquid.h"

void testbench_spgramcf_noise(unsigned int _nfft,
                              int          _wtype,
                              float        _noise_floor)
{
    unsigned int num_samples = 2000*_nfft;  // number of samples to generate
    float        nstd        = powf(10.0f,_noise_floor/20.0f); // noise std. dev.
    float        tol         = 0.5f; // error tolerance [dB]
    if (liquid_autotest_verbose)
        printf("  spgramcf test  (noise): nfft=%6u, wtype=%24s, noise floor=%6.1f\n", _nfft, liquid_window_str[_wtype][1], _noise_floor);

    // create spectral periodogram
    spgramcf q = _wtype == LIQUID_WINDOW_UNKNOWN ? spgramcf_create_default(_nfft) :
                 spgramcf_create(_nfft, _wtype, _nfft/2, _nfft/4);

    unsigned int i;
    for (i=0; i<num_samples; i++)
        spgramcf_push(q, nstd*( randnf() + _Complex_I*randnf() ) * M_SQRT1_2);

    // verify number of samples processed
    CONTEND_EQUALITY(spgramcf_get_num_samples(q),       num_samples);
    CONTEND_EQUALITY(spgramcf_get_num_samples_total(q), num_samples);

    // compute power spectral density output
    float psd[_nfft];
    spgramcf_get_psd(q, psd);

    // verify result
    for (i=0; i<_nfft; i++)
        CONTEND_DELTA(psd[i], _noise_floor, tol)

    // destroy objects
    spgramcf_destroy(q);
}

// test different transform sizes
void autotest_spgramcf_noise_400()  { testbench_spgramcf_noise( 440, 0, -80.0); }
void autotest_spgramcf_noise_1024() { testbench_spgramcf_noise(1024, 0, -80.0); }
void autotest_spgramcf_noise_1200() { testbench_spgramcf_noise(1200, 0, -80.0); }
void autotest_spgramcf_noise_8400() { testbench_spgramcf_noise(8400, 0, -80.0); }

// test different window types
void autotest_spgramcf_noise_hamming        () { testbench_spgramcf_noise(800, LIQUID_WINDOW_HAMMING,        -80.0); }
void autotest_spgramcf_noise_hann           () { testbench_spgramcf_noise(800, LIQUID_WINDOW_HANN,           -80.0); }
void autotest_spgramcf_noise_blackmanharris () { testbench_spgramcf_noise(800, LIQUID_WINDOW_BLACKMANHARRIS, -80.0); }
void autotest_spgramcf_noise_blackmanharris7() { testbench_spgramcf_noise(800, LIQUID_WINDOW_BLACKMANHARRIS7,-80.0); }
void autotest_spgramcf_noise_kaiser         () { testbench_spgramcf_noise(800, LIQUID_WINDOW_KAISER,         -80.0); }
void autotest_spgramcf_noise_flattop        () { testbench_spgramcf_noise(800, LIQUID_WINDOW_FLATTOP,        -80.0); }
void autotest_spgramcf_noise_triangular     () { testbench_spgramcf_noise(800, LIQUID_WINDOW_TRIANGULAR,     -80.0); }
void autotest_spgramcf_noise_rcostaper      () { testbench_spgramcf_noise(800, LIQUID_WINDOW_RCOSTAPER,      -80.0); }
void autotest_spgramcf_noise_kbd            () { testbench_spgramcf_noise(800, LIQUID_WINDOW_KBD,            -80.0); }

void testbench_spgramcf_signal(unsigned int _nfft, int _wtype, float _fc, float _SNRdB)
{
    unsigned int k = 4, m = 12;
    float beta = 0.2f, noise_floor = -80.0f, tol = 0.5f;
    if (liquid_autotest_verbose)
        printf("  spgramcf test (signal): nfft=%6u, wtype=%24s, fc=%6.2f Fs, snr=%6.1f dB\n", _nfft, liquid_window_str[_wtype][1], _fc, _SNRdB);

    // create objects
    spgramcf     q     =  spgramcf_create(_nfft, _wtype, _nfft/2, _nfft/4);
    symstreamcf  gen   = symstreamcf_create_linear(LIQUID_FIRFILT_KAISER,k,m,beta,LIQUID_MODEM_QPSK);
    nco_crcf     mixer = nco_crcf_create(LIQUID_VCO);

    // set parameters
    float nstd = powf(10.0f,noise_floor/20.0f); // noise std. dev.
    symstreamcf_set_gain(gen, powf(10.0f, (noise_floor + _SNRdB - 10*log10f(k))/20.0f));
    nco_crcf_set_frequency(mixer, 2*M_PI*_fc);

    // generate samples and push through spgram object
    unsigned int i, buf_len = 256, num_samples = 0;
    float complex buf[buf_len];
    while (num_samples < 2000*_nfft) {
        // generate block of samples
        symstreamcf_write_samples(gen, buf, buf_len);

        // mix to desired frequency and add noise
        nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
        for (i=0; i<buf_len; i++)
            buf[i] += nstd*(randnf()+_Complex_I*randnf())*M_SQRT1_2;

        // run samples through the spgram object
        spgramcf_write(q, buf, buf_len);
        num_samples += buf_len;
    }

    // determine appropriate indices
    unsigned int i0 = ((unsigned int)roundf((_fc+0.5f)*_nfft)) % _nfft;
    unsigned int ns = (unsigned int)roundf(_nfft*(1.0f-beta)/(float)k); // numer of samples to observe
    float psd_target = 10*log10f(powf(10.0f,noise_floor/10.0f) + powf(10.0f,(noise_floor+_SNRdB)/10.0f));
    //printf("i0=%u, ns=%u (nfft=%u), target=%.3f dB\n", i0, ns, _nfft, psd_target);

    // verify result
    float psd[_nfft];
    spgramcf_get_psd(q, psd);
    //for (i=0; i<_nfft; i++) { printf("%6u %8.2f\n", i, psd[i]); }
    for (i=0; i<ns; i++) {
        unsigned int index = (i0 + i + _nfft - ns/2) % _nfft;
        CONTEND_DELTA(psd[index], psd_target, tol)
    }

    // destroy objects
    spgramcf_destroy(q);
    symstreamcf_destroy(gen);
    nco_crcf_destroy(mixer);
}

void autotest_spgramcf_signal_00() { testbench_spgramcf_signal(800,LIQUID_WINDOW_HAMMING, 0.0f,30.0f); }
void autotest_spgramcf_signal_01() { testbench_spgramcf_signal(800,LIQUID_WINDOW_HAMMING, 0.2f,10.0f); }
void autotest_spgramcf_signal_02() { testbench_spgramcf_signal(800,LIQUID_WINDOW_HANN,    0.2f,10.0f); }
void autotest_spgramcf_signal_03() { testbench_spgramcf_signal(400,LIQUID_WINDOW_KAISER, -0.3f,50.0f); }
void autotest_spgramcf_signal_04() { testbench_spgramcf_signal(640,LIQUID_WINDOW_HAMMING,-0.5f, 0.0f); }
void autotest_spgramcf_signal_05() { testbench_spgramcf_signal(640,LIQUID_WINDOW_HAMMING, 0.1f,-3.0f); }

void autotest_spgramcf_counters()
{
    // create spectral periodogram with specific parameters
    unsigned int nfft=1200, wlen=400, delay=200;
    int wtype = LIQUID_WINDOW_HAMMING;
    float alpha = 0.0123456f;
    spgramcf q = spgramcf_create(nfft, wtype, wlen, delay);

    // check setting bandwidth
    CONTEND_EQUALITY ( spgramcf_set_alpha(q, 0.1),  0 ); // valid
    CONTEND_DELTA    ( spgramcf_get_alpha(q), 0.1, 1e-6f);
    CONTEND_EQUALITY ( spgramcf_set_alpha(q,-7.0), -1 ); // invalid
    CONTEND_DELTA    ( spgramcf_get_alpha(q), 0.1, 1e-6f);
    CONTEND_EQUALITY ( spgramcf_set_alpha(q,alpha),  0); // valid
    CONTEND_DELTA    ( spgramcf_get_alpha(q), alpha, 1e-6f);
    spgramcf_print(q); // test for code coverage

    // check parameters
    CONTEND_EQUALITY( spgramcf_get_nfft(q),       nfft );
    CONTEND_EQUALITY( spgramcf_get_window_len(q), wlen );
    CONTEND_EQUALITY( spgramcf_get_delay(q),      delay);
    CONTEND_EQUALITY( spgramcf_get_alpha(q),      alpha);

    unsigned int block_len = 1117, num_blocks = 1123;
    unsigned int i, num_samples = block_len * num_blocks;
    unsigned int num_transforms = num_samples / delay;
    for (i=0; i<num_samples; i++)
        spgramcf_push(q, randnf() + _Complex_I*randnf());

    // verify number of samples and transforms processed
    CONTEND_EQUALITY(spgramcf_get_num_samples(q),          num_samples);
    CONTEND_EQUALITY(spgramcf_get_num_samples_total(q),    num_samples);
    CONTEND_EQUALITY(spgramcf_get_num_transforms(q),       num_transforms);
    CONTEND_EQUALITY(spgramcf_get_num_transforms_total(q), num_transforms);

    // clear object and run in blocks
    spgramcf_clear(q);
    float complex block[block_len];
    for (i=0; i<block_len; i++)
        block[i] = randnf() + _Complex_I*randnf();
    for (i=0; i<num_blocks; i++)
        spgramcf_write(q, block, block_len);

    // re-verify number of samples and transforms processed
    CONTEND_EQUALITY(spgramcf_get_num_samples(q),          num_samples);
    CONTEND_EQUALITY(spgramcf_get_num_samples_total(q),    num_samples * 2);
    CONTEND_EQUALITY(spgramcf_get_num_transforms(q),       num_transforms);
    CONTEND_EQUALITY(spgramcf_get_num_transforms_total(q), num_transforms * 2);

    // reset object and ensure counters are zero
    spgramcf_reset(q);
    CONTEND_EQUALITY(spgramcf_get_num_samples(q),          0);
    CONTEND_EQUALITY(spgramcf_get_num_samples_total(q),    0);
    CONTEND_EQUALITY(spgramcf_get_num_transforms(q),       0);
    CONTEND_EQUALITY(spgramcf_get_num_transforms_total(q), 0);

    // destroy object(s)
    spgramcf_destroy(q);
}

void autotest_spgramcf_config_errors()
{
    // check that object returns NULL for invalid configurations
    fprintf(stderr,"warning: ignore potential errors here; checking for invalid configurations\n");
    CONTEND_EQUALITY(spgramcf_create(  0, LIQUID_WINDOW_HAMMING,       200, 200)==NULL,1); // nfft too small
    CONTEND_EQUALITY(spgramcf_create(  1, LIQUID_WINDOW_HAMMING,       200, 200)==NULL,1); // nfft too small
    CONTEND_EQUALITY(spgramcf_create(  2, LIQUID_WINDOW_HAMMING,       200, 200)==NULL,1); // window length too large
    CONTEND_EQUALITY(spgramcf_create(400, LIQUID_WINDOW_HAMMING,         0, 200)==NULL,1); // window length too small
    CONTEND_EQUALITY(spgramcf_create(400, LIQUID_WINDOW_UNKNOWN,       200, 200)==NULL,1); // invalid window type
    CONTEND_EQUALITY(spgramcf_create(400, LIQUID_WINDOW_NUM_FUNCTIONS, 200, 200)==NULL,1); // invalid window type
    CONTEND_EQUALITY(spgramcf_create(400, LIQUID_WINDOW_KBD,           201, 200)==NULL,1); // KBD must be even
    CONTEND_EQUALITY(spgramcf_create(400, LIQUID_WINDOW_HAMMING,       200,   0)==NULL,1); // delay too small

    // check that object returns NULL for invalid configurations (default)
    CONTEND_EQUALITY(spgramcf_create_default(0)==NULL,1); // nfft too small
    CONTEND_EQUALITY(spgramcf_create_default(1)==NULL,1); // nfft too small
}

void autotest_spgramcf_standalone()
{
    unsigned int nfft        = 1200;
    unsigned int num_samples = 20*nfft;  // number of samples to generate
    float        noise_floor = -20.0f;
    float        nstd        = powf(10.0f,noise_floor/20.0f); // noise std. dev.

    float complex * buf = (float complex*)malloc(num_samples*sizeof(float complex));
    unsigned int i;
    for (i=0; i<num_samples; i++)
        buf[i] = 0.1f + nstd*(randnf()+_Complex_I*randnf())*M_SQRT1_2;

    float psd[nfft];
    spgramcf_estimate_psd(nfft, buf, num_samples, psd);

    // check mask
    for (i=0; i<nfft; i++) {
        float mask_lo = i ==nfft/2                     ? 2.0f : noise_floor - 3.0f;
        float mask_hi = i > nfft/2-10 && i < nfft/2+10 ? 8.0f : noise_floor + 3.0f;
        if (liquid_autotest_verbose)
            printf("%6u : %8.2f < %8.2f < %8.2f\n", i, mask_lo, psd[i], mask_hi);
        CONTEND_GREATER_THAN( psd[i], mask_lo );
        CONTEND_LESS_THAN   ( psd[i], mask_hi );
    }

    // free memory
    free(buf);
}

