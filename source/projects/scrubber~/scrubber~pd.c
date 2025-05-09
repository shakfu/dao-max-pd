#include "m_pd.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The global variables
 * *******************************************************/
#define MINIMUM_DURATION 000.0
#define DEFAULT_DURATION 5000.0
#define MAXIMUM_DURATION 10000.0

#define SCRUBBER_EMPTY 0
#define SCRUBBER_FULL 1

#define PI 3.1415926535898
#define TWOPI 6.2831853071796

/* The object structure
 * *******************************************************/
typedef struct _scrubber {
    t_object obj;
    t_float x_f;

    float fs;

    float** amplitudes;
    float** phasediffs;
    float* last_phase_in;
    float* last_phase_out;

    float duration_ms;
    float overlap;
    long fftsize;
    long framecount;
    long old_framecount;

    long recording_frame;
    float playback_frame;
    float last_position;

    short acquire_sample;
    short buffer_status;
} t_scrubber;

/* The arguments/inlets/outlets/vectors indexes
 * *******************************/
enum ARGUMENTS { A_DURATION };
enum INLETS { I_REAL, I_IMAG, I_SPEED, I_POSITION, NUM_INLETS };
enum OUTLETS { O_REAL, O_IMAG, NUM_OUTLETS };
enum DSP {
    PERFORM,
    OBJECT,
    INPUT_REAL,
    INPUT_IMAG,
    SPEED,
    POSITION,
    OUTPUT_REAL,
    OUTPUT_IMAG,
    SYNC,
    VECTOR_SIZE,
    NEXT
};

/* The class pointer
 * **********************************************************/
static t_class* scrubber_class;

/* Function prototypes
 * ********************************************************/
void* scrubber_common_new(t_scrubber* x, short argc, t_atom* argv);
void scrubber_free(t_scrubber* x);
void scrubber_dsp(t_scrubber* x, t_signal** sp, short* count);
t_int* scrubber_perform(t_int* w);

/* The object-specific prototypes
 * *********************************************/
void scrubber_init_memory(t_scrubber* x);
void scrubber_sample(t_scrubber* x);
void scrubber_overlap(t_scrubber* x, t_symbol* msg, short argc, t_atom* argv);
void scrubber_resize(t_scrubber* x, t_symbol* msg, short argc, t_atom* argv);

/******************************************************************************/


/* Function prototypes
 * ********************************************************/
void* scrubber_new(t_symbol* s, short argc, t_atom* argv);

/* The 'initialization' routine
 * ***********************************************/
#ifdef WIN32
__declspec(dllexport) void scrubber_tilde_setup(void);
#endif
void scrubber_tilde_setup(void)
{
    /* Initialize the class */
    scrubber_class = class_new(gensym("scrubber~"), (t_newmethod)scrubber_new,
                               (t_method)scrubber_free, sizeof(t_scrubber), 0,
                               A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(scrubber_class, t_scrubber, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(scrubber_class, (t_method)scrubber_dsp, gensym("dsp"), 0);

    /* Bind the object-specific methods */
    class_addmethod(scrubber_class, (t_method)scrubber_sample,
                    gensym("sample"), 0);
    class_addmethod(scrubber_class, (t_method)scrubber_overlap,
                    gensym("overlap"), A_GIMME, 0);
    class_addmethod(scrubber_class, (t_method)scrubber_resize,
                    gensym("resize"), A_GIMME, 0);

    /* Print message to Max window */
    post("scrubber~ • External was loaded");
}

/* The 'new instance' routine
 * *************************************************/
void* scrubber_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_scrubber* x = (t_scrubber*)pd_new(scrubber_class);

    return scrubber_common_new(x, argc, argv);
}

/* The common 'new instance' routine
 * ******************************************/
void* scrubber_common_new(t_scrubber* x, short argc, t_atom* argv)
{
    /* Create inlets */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));
    outlet_new(&x->obj, gensym("signal"));
    outlet_new(&x->obj, gensym("signal"));


    /* Initialize input arguments */
    float duration_ms = DEFAULT_DURATION;

    /* Parse passed arguments */
    if (argc > A_DURATION) {
        duration_ms = atom_getfloatarg(A_DURATION, argc, argv);
    }

    /* Check validity of passed arguments */
    if (duration_ms < MINIMUM_DURATION) {
        duration_ms = MINIMUM_DURATION;
        post("scrubber~ • Invalid argument: Minimum duration set to %.1f[ms]",
             duration_ms);
    } else if (duration_ms > MAXIMUM_DURATION) {
        duration_ms = MAXIMUM_DURATION;
        post("scrubber~ • Invalid argument: Maximum duration set to %.1f[ms]",
             duration_ms);
    }

    /* Initialize some state variables */
    x->fs = sys_getsr();

    x->amplitudes = NULL;
    x->phasediffs = NULL;
    x->last_phase_in = NULL;
    x->last_phase_out = NULL;

    x->duration_ms = duration_ms;
    x->overlap = 8;
    x->fftsize = 1024;
    x->framecount = 1;
    x->old_framecount = 0;

    x->recording_frame = 0;
    x->playback_frame = 0;
    x->last_position = 0;

    x->acquire_sample = 0;
    x->buffer_status = SCRUBBER_EMPTY;

    scrubber_init_memory(x);

    /* Print message to Max window */
    post("scrubber~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void scrubber_free(t_scrubber* x)
{
    /* Free allocated dynamic memory */
    if (x->amplitudes != NULL) {
        for (int ii = 0; ii < x->framecount; ii++) {
            free(x->amplitudes[ii]);
            free(x->phasediffs[ii]);
        }
        free(x->amplitudes);
        free(x->phasediffs);
        free(x->last_phase_in);
        free(x->last_phase_out);
    }

    /* Print message to Max window */
    post("scrubber~ • Memory was freed");
}

/* The object-specific methods
 * ************************************************/
void scrubber_init_memory(t_scrubber* x)
{
    long framecount = x->framecount;
    if (framecount <= 0) {
        post("scrubber~ • bad frame count: %d", framecount);
        return;
    }

    long framesize;
    framesize = x->fftsize / 2 + 1;

    if (framesize <= 0) {
        post("scrubber~ • bad frame size: %d", framesize);
        return;
    }

    x->buffer_status = SCRUBBER_EMPTY;

    long bytesize;

    if (x->amplitudes == NULL) {
        bytesize = framecount * sizeof(float*);
        x->amplitudes = (float**)malloc(bytesize);
        x->phasediffs = (float**)malloc(bytesize);

        bytesize = framesize * sizeof(float);
        x->last_phase_in = (float*)malloc(bytesize);
        x->last_phase_out = (float*)malloc(bytesize);

    } else {
        for (int ii = 0; ii < x->old_framecount; ii++) {
            free(x->amplitudes[ii]);
            free(x->phasediffs[ii]);
        }

        bytesize = framecount * sizeof(float*);
        x->amplitudes = (float**)realloc(x->amplitudes, bytesize);
        x->phasediffs = (float**)realloc(x->phasediffs, bytesize);

        bytesize = framesize * sizeof(float);
        x->last_phase_in = (float*)realloc(x->last_phase_in, bytesize);
        x->last_phase_out = (float*)realloc(x->last_phase_out, bytesize);
    }

    bytesize = framesize * sizeof(float);
    for (int ii = 0; ii < framecount; ii++) {
        x->amplitudes[ii] = (float*)malloc(bytesize);
        x->phasediffs[ii] = (float*)malloc(bytesize);

        memset(x->amplitudes[ii], 0, bytesize);
        memset(x->phasediffs[ii], 0, bytesize);
    }

    memset(x->last_phase_in, 0, bytesize);
    memset(x->last_phase_out, 0, bytesize);

    x->old_framecount = framecount;
}

void scrubber_sample(t_scrubber* x)
{
    x->recording_frame = 0;
    x->playback_frame = 0;

    x->acquire_sample = 1;
    x->buffer_status = SCRUBBER_EMPTY;
}

void scrubber_overlap(t_scrubber* x, t_symbol* msg, short argc, t_atom* argv)
{
    if (argc >= 1) {
        float new_overlap = atom_getfloatarg(0, argc, argv);
        if (new_overlap <= 0) {
            post("scrubber~ • bad overlap: %f", new_overlap);
            return;
        }

        if (x->overlap != new_overlap) {
            x->overlap = new_overlap;
            scrubber_init_memory(x);
        }
    }
}

void scrubber_resize(t_scrubber* x, t_symbol* msg, short argc, t_atom* argv)
{
    if (argc >= 1) {
        float new_duration = atom_getfloatarg(0, argc, argv);
        if (new_duration < 0.0) {
            post("scrubber~ • bad duration: %.1f[ms]", new_duration);
            return;
        }

        if (x->duration_ms != new_duration) {
            x->duration_ms = new_duration;
            x->old_framecount = x->framecount;
            x->framecount = (x->duration_ms * 0.001 * x->fs)
                * (x->overlap / (x->fftsize * 2));

            scrubber_init_memory(x);
        }
    }
}

/* The 'DSP' method
 * ***********************************************************/

void scrubber_dsp(t_scrubber* x, t_signal** sp, short* count)
{
    /* Adjust to changes in the sampling rate */
    float new_fs;
    long new_fftsize;

    new_fs = sys_getsr();
    new_fftsize = sp[0]->s_n;

    long new_framecount = (x->duration_ms * 0.001 * new_fs)
        * (x->overlap / new_fftsize);

    if (x->fs != new_fs || x->fftsize != new_fftsize
        || x->framecount != new_framecount) {

        x->fs = new_fs;
        x->fftsize = new_fftsize;
        x->framecount = new_framecount;

        scrubber_init_memory(x);
    }

    /* Attach the object to the DSP chain */
    dsp_add(scrubber_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
            sp[6]->s_vec, sp[0]->s_n);

    /* Print message to Max window */
    post("scrubber~ • Executing 32-bit perform routine");
}

/* The 'perform' routine
 * ******************************************************/
t_int* scrubber_perform(t_int* w)
{
    /* Copy the object pointer */
    t_scrubber* x = (t_scrubber*)w[OBJECT];

    /* Copy signal pointers */
    t_float* input_real = (t_float*)w[INPUT_REAL];
    t_float* input_imag = (t_float*)w[INPUT_IMAG];
    t_float* speed = (t_float*)w[SPEED];
    t_float* position = (t_float*)w[POSITION];
    t_float* output_real = (t_float*)w[OUTPUT_REAL];
    t_float* output_imag = (t_float*)w[OUTPUT_IMAG];
    t_float* sync = (t_float*)w[SYNC];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Load state variables */
    float** amplitudes = x->amplitudes;
    float** phasediffs = x->phasediffs;

    float* last_phase_in = x->last_phase_in;
    float* last_phase_out = x->last_phase_out;

    long framecount = x->framecount;

    long recording_frame = x->recording_frame;
    float playback_frame = x->playback_frame;
    float last_position = x->last_position;

    short acquire_samples = x->acquire_sample;
    short buffer_status = x->buffer_status;

    /* Perform the DSP loop */
    float local_real;
    float local_imag;
    float local_magnitude;
    float local_phase;
    float phasediff;
    float sync_val;

    int framesize;
    framesize = (int)(n / 2) + 1;

    // mode: analysis
    if (acquire_samples) {
        sync_val = (float)recording_frame / (float)framecount;

        for (int ii = 0; ii < framesize; ii++) {
            local_real = input_real[ii];
            local_imag = (ii == 0 || ii == framesize - 1) ? 0.0
                                                          : input_imag[ii];

            // functionality of cartopol~
            local_magnitude = hypotf(local_real, local_imag);
            local_phase = -atan2(local_imag, local_real);

            // functionality of framedelta~
            phasediff = local_phase - last_phase_in[ii];
            last_phase_in[ii] = local_phase;

            // functionality of phasewrap~
            while (phasediff > PI) {
                phasediff -= TWOPI;
            }
            while (phasediff < -PI) {
                phasediff += TWOPI;
            }

            // record magnitudes and phases
            amplitudes[recording_frame][ii] = local_magnitude;
            phasediffs[recording_frame][ii] = phasediff;
        }

        // playback silence
        for (int ii = 0; ii < n; ii++) {
            output_real[ii] = 0.0;
            output_imag[ii] = 0.0;
            sync[ii] = sync_val;
        }

        recording_frame++;
        if (recording_frame >= framecount) {
            x->acquire_sample = 0;
            x->buffer_status = SCRUBBER_FULL;
        }

        // mode: synthesis
    } else if (buffer_status == SCRUBBER_FULL) {
        sync_val = playback_frame / (float)framecount;

        if (*position != last_position && *position >= 0.0
            && *position <= 1.0) {

            last_position = *position;
            playback_frame = last_position * (float)(framecount - 1);
        }

        playback_frame += *speed;
        while (playback_frame < 0.0) {
            playback_frame += framecount;
        }
        while (playback_frame >= framecount) {
            playback_frame -= framecount;
        }

        int int_playback_frame = floor(playback_frame);
        for (int ii = 0; ii < framesize; ii++) {
            local_magnitude = amplitudes[int_playback_frame][ii];
            local_phase = phasediffs[int_playback_frame][ii];

            // functionality of frameaccum~
            last_phase_out[ii] += local_phase;
            local_phase = last_phase_out[ii];

            // functionality of poltocar~
            local_real = local_magnitude * cos(local_phase);
            local_imag = -local_magnitude * sin(local_phase);

            // playback real and imaginary part
            output_real[ii] = local_real;
            output_imag[ii] = (ii == 0 || ii == framesize - 1) ? 0.0
                                                               : local_imag;
            sync[ii] = sync_val;
        }

        for (int ii = framesize; ii < n; ii++) {
            output_real[ii] = 0.0;
            output_imag[ii] = 0.0;
            sync[ii] = sync_val;
        }

        // mode: stand by - waiting to start sampling
    } else {
        for (int ii = 0; ii < n; ii++) {
            output_real[ii] = 0.0;
            output_imag[ii] = 0.0;
            sync[ii] = 0.0;
        }
    }

    /* Update state variables */
    x->recording_frame = recording_frame;
    x->playback_frame = playback_frame;
    x->last_position = last_position;

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
