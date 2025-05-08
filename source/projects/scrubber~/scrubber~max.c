#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The global variables *******************************************************/
#define MINIMUM_DURATION 000.0
#define DEFAULT_DURATION 5000.0
#define MAXIMUM_DURATION 10000.0

#define SCRUBBER_EMPTY 0
#define SCRUBBER_FULL 1

#define PI 3.1415926535898
#define TWOPI 6.2831853071796

/* The object structure *******************************************************/
typedef struct _scrubber {
    t_pxobject obj;

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

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { A_DURATION };
enum INLETS { I_REAL, I_IMAG, I_SPEED, I_POSITION, NUM_INLETS };
enum OUTLETS { O_REAL, O_IMAG, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT_REAL, INPUT_IMAG, SPEED, POSITION, OUTPUT_REAL, OUTPUT_IMAG, SYNC,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *scrubber_class;

/* Function prototypes ********************************************************/
void *scrubber_common_new(t_scrubber *x, short argc, t_atom *argv);
void scrubber_free(t_scrubber *x);
void scrubber_dsp64(t_scrubber* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void scrubber_perform64(t_scrubber* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);
/* The object-specific prototypes *********************************************/
void scrubber_init_memory(t_scrubber *x);
void scrubber_sample(t_scrubber *x);
void scrubber_overlap(t_scrubber *x, t_symbol *msg, short argc, t_atom *argv);
void scrubber_resize(t_scrubber *x, t_symbol *msg, short argc, t_atom *argv);

/* Function prototypes ********************************************************/
void *scrubber_new(t_symbol *s, short argc, t_atom *argv);

void scrubber_float(t_scrubber *x, double farg);
void scrubber_assist(t_scrubber *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    scrubber_class = class_new("scrubber~",
                               (method)scrubber_new,
                               (method)scrubber_free,
                               sizeof(t_scrubber), 0, A_GIMME, 0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(scrubber_class, (method)scrubber_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(scrubber_class, (method)scrubber_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and outlets */
    class_addmethod(scrubber_class, (method)scrubber_assist, "assist", A_CANT, 0);

    /* Bind the object-specific methods */
    class_addmethod(scrubber_class, (method)scrubber_sample, "sample", 0);
    class_addmethod(scrubber_class, (method)scrubber_overlap, "overlap", A_FLOAT, 0);
    class_addmethod(scrubber_class, (method)scrubber_resize, "resize", A_FLOAT, 0);

    /* Add standard Max methods to the class */
    class_dspinit(scrubber_class);

    /* Register the class with Max */
    class_register(CLASS_BOX, scrubber_class);

    /* Print message to Max window */
    object_post(NULL, "scrubber~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine *************************************************/
void *scrubber_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_scrubber *x = (t_scrubber *)object_alloc(scrubber_class);

    return scrubber_common_new(x, argc, argv);
}

/* The 'float' method *********************************************************/
void scrubber_float(t_scrubber *x, double farg)
{
    // nothing

    object_post((t_object *)x, "Receiving floats");
}

/* The 'assist' method ********************************************************/
void scrubber_assist(t_scrubber *x, void *b, long msg, long arg, char *dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
            case I_REAL: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Real part"); break;
            case I_IMAG: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Imaginary part"); break;
            case I_SPEED: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Speed"); break;
            case I_POSITION: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Position"); break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
            case O_REAL: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Real part"); break;
            case O_IMAG: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Imaginary part"); break;
        }
    }
}


/* The common 'new instance' routine ******************************************/
void *scrubber_common_new(t_scrubber *x, short argc, t_atom *argv)
{
    /* Create inlets */
    dsp_setup((t_pxobject *)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object *)x, "signal");
    outlet_new((t_object *)x, "signal");
    outlet_new((t_object *)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;

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
    }
    else if (duration_ms > MAXIMUM_DURATION) {
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

/* The 'free instance' routine ************************************************/
void scrubber_free(t_scrubber *x)
{
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject *)x);

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

/* The object-specific methods ************************************************/
void scrubber_init_memory(t_scrubber *x)
{
    long framecount = x->framecount;
    if (framecount <= 0) {
        post("scrubber~ • bad frame count: %d", framecount);
        return;
    }

    long framesize;
    framesize = x->fftsize / 2;
    if (framesize <= 0) {
        post("scrubber~ • bad frame size: %d", framesize);
        return;
    }

    x->buffer_status = SCRUBBER_EMPTY;

    long bytesize;

    if (x->amplitudes == NULL) {
        bytesize = framecount * sizeof(float *);
        x->amplitudes = (float **)malloc(bytesize);
        x->phasediffs = (float **)malloc(bytesize);

        bytesize = framesize * sizeof(float);
        x->last_phase_in = (float *)malloc(bytesize);
        x->last_phase_out = (float *)malloc(bytesize);

    } else {
        for (int ii = 0; ii < x->old_framecount; ii++) {
            free(x->amplitudes[ii]);
            free(x->phasediffs[ii]);
        }

        bytesize = framecount * sizeof(float *);
        x->amplitudes = (float **)realloc(x->amplitudes, bytesize);
        x->phasediffs = (float **)realloc(x->phasediffs, bytesize);

        bytesize = framesize * sizeof(float);
        x->last_phase_in = (float *)realloc(x->last_phase_in, bytesize);
        x->last_phase_out = (float *)realloc(x->last_phase_out, bytesize);
    }

    bytesize = framesize * sizeof(float);
    for (int ii = 0; ii < framecount; ii++) {
        x->amplitudes[ii] = (float *)malloc(bytesize);
        x->phasediffs[ii] = (float *)malloc(bytesize);

        memset(x->amplitudes[ii], 0, bytesize);
        memset(x->phasediffs[ii], 0, bytesize);
    }

    memset(x->last_phase_in, 0, bytesize);
    memset(x->last_phase_out, 0, bytesize);

    x->old_framecount = framecount;
}

void scrubber_sample(t_scrubber *x)
{
    x->recording_frame = 0;
    x->playback_frame = 0;

    x->acquire_sample = 1;
    x->buffer_status = SCRUBBER_EMPTY;
}

void scrubber_overlap(t_scrubber *x, t_symbol *msg, short argc, t_atom *argv)
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

void scrubber_resize(t_scrubber *x, t_symbol *msg, short argc, t_atom *argv)
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
            x->framecount =
                (x->duration_ms * 0.001 * x->fs) * (x->overlap / (x->fftsize * 2));

            scrubber_init_memory(x);
        }
    }
}

/* The 'DSP' method ***********************************************************/


void scrubber_dsp64(t_scrubber* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    /* Adjust to changes in the sampling rate */
    float new_fs;
    long new_fftsize;

    new_fs = samplerate;
    new_fftsize = maxvectorsize * 2;

    long new_framecount =
        (x->duration_ms * 0.001 * new_fs) * (x->overlap / new_fftsize);

    if (x->fs != new_fs ||
        x->fftsize != new_fftsize ||
        x->framecount != new_framecount) {

        x->fs = new_fs;
        x->fftsize = new_fftsize;
        x->framecount = new_framecount;

        scrubber_init_memory(x);
    }

    /* Attach the object to the DSP chain */
    object_method(dsp64, gensym("dsp_add64"), x, scrubber_perform64, 0, NULL);

    /* Print message to Max window */
    post("scrubber~ • Executing 64-bit perform routine");
}

void scrubber_perform64(t_scrubber* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    /* Copy signal pointers */
    t_double* input_real = ins[0];
    t_double* input_imag = ins[1];
    t_double* speed = ins[2];
    t_double* position = ins[3];

    t_double* output_real = outs[0];
    t_double* output_imag = outs[1];
    t_double* sync = outs[2];
    int n = sampleframes;

    /* Load state variables */
    float **amplitudes = x->amplitudes;
    float **phasediffs = x->phasediffs;

    float *last_phase_in = x->last_phase_in;
    float *last_phase_out = x->last_phase_out;

    long framecount = x->framecount;

    long recording_frame = x->recording_frame;
    double playback_frame = x->playback_frame;
    double last_position = x->last_position;

    short acquire_samples = x->acquire_sample;
    short buffer_status = x->buffer_status;

    /* Perform the DSP loop */
    double local_real;
    double local_imag;
    double local_magnitude;
    double local_phase;
    double phasediff;
    double sync_val;

    int framesize;
    framesize = (int)n;

    // mode: analysis
    if (acquire_samples) {
        sync_val = (double)recording_frame / (double)framecount;

        for (int ii = 0; ii < framesize; ii++) {
            local_real = input_real[ii];
            local_imag = (ii == 0 || ii == framesize-1) ? 0.0 : input_imag[ii];

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
        sync_val = playback_frame / (double)framecount;

        if (*position != last_position &&
            *position >= 0.0 &&
            *position <= 1.0) {

            last_position = *position;
            playback_frame = last_position * (double)(framecount - 1);
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
            output_imag[ii] = (ii == 0 || ii == framesize-1) ? 0.0 : local_imag;
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

}

