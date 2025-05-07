#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

/* The global variables *******************************************************/
#define NUM_POINTS 12

#define INITIAL_FREQ 440.0
#define MINIMUM_FREQ 100.0
#define MAXIMUM_FREQ 800.0

#define MINIMUM_AMP_DEV 0.0
#define DEFAULT_AMP_DEV 0.001
#define MAXIMUM_AMP_DEV 2.0

#define MINIMUM_DUR_DEV 0.0
#define DEFAULT_DUR_DEV 0.001

/* The object structure *******************************************************/
typedef struct _dynstoch {
    t_pxobject obj;
    float a_ampdev;
    float a_durdev;
    float a_freqrange[2];
    float a_newfreq;

    int num_points;
    float *amplitudes;
    float *durations;

    float sr;
    float freq;

    float min_freq;
    float max_freq;
    float min_duration;
    float max_duration;

    float amplitude_deviation;
    float duration_deviation;

    int current_segment;
    int remaining_samples;

    int total_length;
    short first_time;
} t_dynstoch;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT, NUM_INLETS };
enum OUTLETS { O_OUTPUT, O_FREQUENCY, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT1, OUTPUT1, FREQUENCY,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *dynstoch_class;

/* Function prototypes ********************************************************/
void *dynstoch_common_new(t_dynstoch *x, short argc, t_atom *argv);
void dynstoch_free(t_dynstoch *x);

void dynstoch_dsp64(t_dynstoch* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void dynstoch_perform64(t_dynstoch* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

/* The object-specific methods ************************************************/
void dynstoch_ampdev(t_dynstoch *x, float ampdev);
void dynstoch_durdev(t_dynstoch *x, float durdev);
void dynstoch_setfreq(t_dynstoch *x, float new_freq);
void dynstoch_freqrange(t_dynstoch *x, float min_freq, float max_freq);

float dynstoch_rand(float min, float max);
void dynstoch_initwave(t_dynstoch *x);
void dynstoch_recalculate(t_dynstoch *x);

/* Function prototypes ********************************************************/
void *dynstoch_new(t_symbol *s, short argc, t_atom *argv);

t_max_err a_ampdev_set(t_dynstoch *x, void *attr, long ac, t_atom *av);
t_max_err a_durdev_set(t_dynstoch *x, void *attr, long ac, t_atom *av);
t_max_err a_setfreq_set(t_dynstoch *x, void *attr, long ac, t_atom *av);
t_max_err a_freqrange_set(t_dynstoch *x, void *attr, long ac, t_atom *av);
void dynstoch_float(t_dynstoch *x, double farg);
void dynstoch_assist(t_dynstoch *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    dynstoch_class = class_new("dynstoch~",
                               (method)dynstoch_new,
                               (method)dynstoch_free,
                               sizeof(t_dynstoch), 0, A_GIMME, 0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(dynstoch_class, (method)dynstoch_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(dynstoch_class, (method)dynstoch_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and outlets */
    class_addmethod(dynstoch_class, (method)dynstoch_assist, "assist", A_CANT, 0);

    /* Bind the object-specific methods */
    // handled using attributes

    /* Add standard Max methods to the class */
    class_dspinit(dynstoch_class);

    /* Bind the attributes */
    CLASS_ATTR_FLOAT(dynstoch_class, "ampdev", 0, t_dynstoch, a_ampdev);
    CLASS_ATTR_LABEL(dynstoch_class, "ampdev", 0, "Amplitude deviation");
    CLASS_ATTR_ORDER(dynstoch_class, "ampdev", 0, "1");
    CLASS_ATTR_ACCESSORS(dynstoch_class, "ampdev", NULL, a_ampdev_set);

    CLASS_ATTR_FLOAT(dynstoch_class, "durdev", 0, t_dynstoch, a_durdev);
    CLASS_ATTR_LABEL(dynstoch_class, "durdev", 0, "Duration deviation");
    CLASS_ATTR_ORDER(dynstoch_class, "durdev", 0, "2");
    CLASS_ATTR_ACCESSORS(dynstoch_class, "durdev", NULL, a_durdev_set);

    CLASS_ATTR_FLOAT(dynstoch_class, "setfreq", 0, t_dynstoch, a_newfreq);
    CLASS_ATTR_LABEL(dynstoch_class, "setfreq", 0, "Frequency");
    CLASS_ATTR_ORDER(dynstoch_class, "setfreq", 0, "3");
    CLASS_ATTR_ACCESSORS(dynstoch_class, "setfreq", NULL, a_setfreq_set);

    CLASS_ATTR_FLOAT_ARRAY(dynstoch_class, "freqrange", 0, t_dynstoch, a_freqrange, 2);
    CLASS_ATTR_LABEL(dynstoch_class, "freqrange", 0, "Frequency range");
    CLASS_ATTR_ORDER(dynstoch_class, "freqrange", 0, "4");
    CLASS_ATTR_ACCESSORS(dynstoch_class, "freqrange", NULL, a_freqrange_set);

    /* Register the class with Max */
    class_register(CLASS_BOX, dynstoch_class);

    /* Print message to Max window */
    object_post(NULL, "dynstoch~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine *************************************************/
void *dynstoch_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_dynstoch *x = (t_dynstoch *)object_alloc(dynstoch_class);

    return dynstoch_common_new(x, argc, argv);
}

/******************************************************************************/






/* The 'setter' and 'getter' methods for attributes ***************************/
t_max_err a_ampdev_set(t_dynstoch *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_ampdev = atom_getfloat(av);
        dynstoch_ampdev(x, x->a_ampdev);
    }

    return MAX_ERR_NONE;
}

t_max_err a_durdev_set(t_dynstoch *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_durdev = atom_getfloat(av);
        dynstoch_durdev(x, x->a_durdev);
    }

    return MAX_ERR_NONE;
}

t_max_err a_setfreq_set(t_dynstoch *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_newfreq = atom_getfloat(av);
        dynstoch_setfreq(x, x->a_newfreq);
    }

    return MAX_ERR_NONE;
}

t_max_err a_freqrange_set(t_dynstoch *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        atom_getfloat_array(ac, av, 2, x->a_freqrange);
        dynstoch_freqrange(x, x->a_freqrange[0], x->a_freqrange[1]);
    }

    return MAX_ERR_NONE;
}

/* The 'float' method *********************************************************/
void dynstoch_float(t_dynstoch *x, double farg)
{
    // nothing
}

/* The 'assist' method ********************************************************/
void dynstoch_assist(t_dynstoch *x, void *b, long msg, long arg, char *dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
            case I_INPUT: sprintf(dst, "(messages) Object's messages"); break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
            case O_OUTPUT: sprintf(dst, "(signal) Output"); break;
            case O_FREQUENCY: sprintf(dst, "(signal) Frequency"); break;
        }
    }
}


/* The common 'new instance' routine ******************************************/
void *dynstoch_common_new(t_dynstoch *x, short argc, t_atom *argv)
{
    /* Create inlets */
    dsp_setup((t_pxobject *)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object *)x, "signal");
    outlet_new((t_object *)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;

    /* Initialize state variables */
    x->num_points = NUM_POINTS;
    x->amplitudes = (float *)malloc((x->num_points + 1) * sizeof(float));
    x->durations = (float *)malloc(x->num_points * sizeof(float));

    if (x->amplitudes == NULL ||
        x->durations == NULL) {
        error("dynstoch~ • Cannot allocate memory for this object");
        return NULL;
    }

    x->freq = INITIAL_FREQ;
    x->min_freq = MINIMUM_FREQ;
    x->max_freq = MAXIMUM_FREQ;
    x->amplitude_deviation = DEFAULT_AMP_DEV;
    x->duration_deviation = DEFAULT_DUR_DEV;
    x->first_time = 1;

    /* Process the attributes */
#ifdef TARGET_IS_MAX
    x->a_ampdev = x->amplitude_deviation;
    x->a_durdev = x->duration_deviation;
    x->a_newfreq = x->freq;
    x->a_freqrange[0] = x->min_freq;
    x->a_freqrange[1] = x->max_freq;
    attr_args_process(x, argc, argv);
#endif

    /* Print message to Max window */
    post("dynstoch~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void dynstoch_free(t_dynstoch *x)
{
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject *)x);

    /* Free allocated dynamic memory */
    free(x->amplitudes);
    free(x->durations);

    /* Print message to Max window */
    post("dynstoch~ • Memory was freed");
}

/* The object-specific methods ************************************************/
void dynstoch_ampdev(t_dynstoch *x, float ampdev)
{
    if (ampdev < MINIMUM_AMP_DEV) {
        ampdev = MINIMUM_AMP_DEV;
        error("dynstoch~ • Invalid attribute: Amplitude deviation set to %.4f", MINIMUM_AMP_DEV);
    }
    if (ampdev > MAXIMUM_AMP_DEV) {
        ampdev = MAXIMUM_AMP_DEV;
        error("dynstoch~ • Invalid attribute: Amplitude deviation set to %.4f", MAXIMUM_AMP_DEV);
    }
    x->amplitude_deviation = ampdev;
}

void dynstoch_durdev(t_dynstoch *x, float durdev)
{
    if (durdev < MINIMUM_DUR_DEV) {
        durdev = MINIMUM_DUR_DEV;
        error("dynstoch~ • Invalid attribute: Duration deviation set to %.4f", MINIMUM_DUR_DEV);
    }
    x->duration_deviation = durdev;
}

void dynstoch_setfreq(t_dynstoch *x, float new_freq)
{
    if (new_freq < x->min_freq || x->max_freq < new_freq) {
        post("dynstoch~ • Frequency requested out of range: %f", new_freq);
        return;
    }

    float total_length = x->sr / new_freq;
    long segment_duration = total_length / (float)x->num_points;
    long difference = total_length - (segment_duration * x->num_points);

    for (int ii = 0; ii < x->num_points; ii++) {
        x->durations[ii] = segment_duration;
    }

    int jj = 0;
    while (difference-- > 0) {
        x->durations[jj++]++;
        jj %= x->num_points;
    }

    x->total_length = 0;
    for (int ii = 0; ii < x->num_points; ii++) {
        x->total_length += x->durations[ii];
    }
    x->freq = x->sr / x->total_length;
}

void dynstoch_freqrange(t_dynstoch *x, float min_freq, float max_freq)
{
    if (min_freq <= 0 || max_freq <= 0) {
        return;
    }

    x->min_freq = min_freq;
    x->max_freq = max_freq;

    x->min_duration = x->sr / max_freq;
    x->max_duration = x->sr / min_freq;
}

float dynstoch_rand(float min, float max)
{
    return (rand() % 32768) / 32767.0 * (max - min) + min;
}

void dynstoch_initwave(t_dynstoch *x)
{
    x->total_length = x->sr / x->freq;

    for (int ii = 0; ii < x->num_points; ii++) {
        x->amplitudes[ii] = dynstoch_rand(-1.0, 1.0);
        x->durations[ii] = x->total_length / x->num_points;
    }

    x->current_segment = 0;
    x->amplitudes[x->num_points] = x->amplitudes[0];
    x->remaining_samples = x->durations[0];
}

void dynstoch_recalculate(t_dynstoch *x)
{
    float amplitude_adjustment;
    float duration_adjustment;

    x->total_length = 0;
    for (int ii = 0; ii < x->num_points; ii++) {
        amplitude_adjustment =
            dynstoch_rand(-x->amplitude_deviation, x->amplitude_deviation);
        duration_adjustment =
            dynstoch_rand(-x->duration_deviation, x->duration_deviation);

        /* Adjust amplitudes and durations */
        x->amplitudes[ii] += amplitude_adjustment;
        x->durations[ii] += duration_adjustment;

        /* Mirror amplitudes */
        while (x->amplitudes[ii] > 1.0) {
            x->amplitudes[ii] = 2.0 - x->amplitudes[ii];
        }
        while (x->amplitudes[ii] < -1.0) {
            x->amplitudes[ii] = -2.0 - x->amplitudes[ii];
        }

        /* Limit durations */
        if (x->durations[ii] < 1) {
            x->durations[ii] = 1;
        }
        if (x->durations[ii] > x->sr * 50e-3 / x->num_points) {
            x->durations[ii] = x->sr * 50e-3 / x->num_points;
        }

        x->total_length += x->durations[ii];
    }

    /* Force waveform period within frequency boundaries */
    int difference;
    if (x->total_length > x->max_duration){
        difference = x->total_length - x->max_duration;
        x->total_length = x->max_duration;
        for (int ii = 0; ii < difference; ii++){
            if (x->durations[ii % x->num_points] > 1) {
                x->durations[ii % x->num_points] -= 1;
            }
        }
    } else if (x->total_length < x->min_duration) {
        difference = x->min_duration - x->total_length;
        x->total_length = x->min_duration;
        for (int ii = 0; ii < difference; ii++){
            x->durations[ii % x->num_points] += 1;
        }
    }

    /* Limit minimum durations and recalculate total length and frequency */
    x->total_length = 0;
    for (int ii = 0; ii < x->num_points; ii++) {
        if (x->durations[ii] < 1) {
            x->durations[ii] = 1;
        }
        x->total_length += x->durations[ii];
    }

    x->freq = x->sr / x->total_length;

    x->current_segment = 0;
    x->amplitudes[x->num_points] = x->amplitudes[0];
    x->remaining_samples = x->durations[0];
}

/******************************************************************************/

void dynstoch_dsp64(t_dynstoch* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    if (samplerate == 0) {
        error("dynstoch~ • Sampling rate is equal to zero!");
        return;
    }

    /* Initialize state variables */
    x->sr = samplerate;

    /* Initialize waveform */
    dynstoch_freqrange(x, x->min_freq, x->max_freq);
    if (x->first_time) {
        dynstoch_initwave(x);
        x->first_time = 0;
    }

    object_method(dsp64, gensym("dsp_add64"), x, dynstoch_perform64, 0, NULL);

}

void dynstoch_perform64(t_dynstoch* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* input = ins[0];
    t_double* output = outs[0];
    t_double* frequency = outs[1];
    int n = sampleframes;

    /* Load state variables */
    int num_points = x->num_points;
    float *amplitudes = x->amplitudes;
    float *durations = x->durations;

    int current_segment = x->current_segment;
    float remaining_samples = x->remaining_samples;

    /* Perform the DSP loop */
    float amplitude1 = amplitudes[current_segment + 0];
    float amplitude2 = amplitudes[current_segment + 1];

    float frac;
    float sample;
    while (n--) {
        if (remaining_samples < 1) {
            current_segment++;
            if (current_segment == num_points) {
                dynstoch_recalculate(x);
                current_segment = 0;
            }

            remaining_samples = x->durations[current_segment];
            amplitude1 = x->amplitudes[current_segment + 0];
            amplitude2 = x->amplitudes[current_segment + 1];
        }

        frac = (remaining_samples - 1) / durations[current_segment];
        sample = (amplitude1 - amplitude2) * frac + amplitude2;

        *output++ = sample;
        *frequency++ = x->freq;

        remaining_samples--;
    }

    /* Update state variables */
    x->current_segment = current_segment;
    x->remaining_samples = remaining_samples;
}


/******************************************************************************/
