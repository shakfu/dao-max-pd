#include "m_pd.h"`

#include <math.h>
#include <stdlib.h>
#include <time.h>

/* The global variables
 * *******************************************************/
#define NUM_POINTS 12

#define INITIAL_FREQ 440.0
#define MINIMUM_FREQ 100.0
#define MAXIMUM_FREQ 800.0

#define MINIMUM_AMP_DEV 0.0
#define DEFAULT_AMP_DEV 0.001
#define MAXIMUM_AMP_DEV 2.0

#define MINIMUM_DUR_DEV 0.0
#define DEFAULT_DUR_DEV 0.001

/* The object structure
 * *******************************************************/
typedef struct _dynstoch {
    t_object obj;
    t_float x_f;

    int num_points;
    float* amplitudes;
    float* durations;

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

/* The arguments/inlets/outlets/vectors indexes
 * *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT, NUM_INLETS };
enum OUTLETS { O_OUTPUT, O_FREQUENCY, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT, INPUT1, OUTPUT1, FREQUENCY, VECTOR_SIZE, NEXT };

/* The class pointer
 * **********************************************************/
static t_class* dynstoch_class;

/* Function prototypes
 * ********************************************************/
void* dynstoch_common_new(t_dynstoch* x, short argc, t_atom* argv);
void dynstoch_free(t_dynstoch* x);

void dynstoch_dsp(t_dynstoch* x, t_signal** sp, short* count);
t_int* dynstoch_perform(t_int* w);

/* The object-specific methods
 * ************************************************/
void dynstoch_ampdev(t_dynstoch* x, float ampdev);
void dynstoch_durdev(t_dynstoch* x, float durdev);
void dynstoch_setfreq(t_dynstoch* x, float new_freq);
void dynstoch_freqrange(t_dynstoch* x, float min_freq, float max_freq);

float dynstoch_rand(float min, float max);
void dynstoch_initwave(t_dynstoch* x);
void dynstoch_recalculate(t_dynstoch* x);

/******************************************************************************/


/* Function prototypes
 * ********************************************************/
void* dynstoch_new(t_symbol* s, short argc, t_atom* argv);

/* The 'initialization' routine
 * ***********************************************/
#ifdef WIN32
__declspec(dllexport) void dynstoch_tilde_setup(void);
#endif
void dynstoch_tilde_setup(void)
{
    /* Initialize the class */
    dynstoch_class = class_new(gensym("dynstoch~"), (t_newmethod)dynstoch_new,
                               (t_method)dynstoch_free, sizeof(t_dynstoch), 0,
                               A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(dynstoch_class, t_dynstoch, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(dynstoch_class, (t_method)dynstoch_dsp, gensym("dsp"), 0);

    /* Bind the object-specific methods */
    class_addmethod(dynstoch_class, (t_method)dynstoch_ampdev,
                    gensym("ampdev"), A_FLOAT, 0);
    class_addmethod(dynstoch_class, (t_method)dynstoch_durdev,
                    gensym("durdev"), A_FLOAT, 0);
    class_addmethod(dynstoch_class, (t_method)dynstoch_setfreq,
                    gensym("setfreq"), A_FLOAT, 0);
    class_addmethod(dynstoch_class, (t_method)dynstoch_freqrange,
                    gensym("freqrange"), A_FLOAT, A_FLOAT, 0);

    /* Print message to Max window */
    post("dynstoch~ • External was loaded");
}

/* The 'new instance' routine
 * *************************************************/
void* dynstoch_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_dynstoch* x = (t_dynstoch*)pd_new(dynstoch_class);

    return dynstoch_common_new(x, argc, argv);
}

/* The common 'new instance' routine
 * ******************************************/
void* dynstoch_common_new(t_dynstoch* x, short argc, t_atom* argv)
{
    /* Create inlets */
    // inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));
    outlet_new(&x->obj, gensym("signal"));

    /* Initialize state variables */
    x->num_points = NUM_POINTS;
    x->amplitudes = (float*)malloc((x->num_points + 1) * sizeof(float));
    x->durations = (float*)malloc(x->num_points * sizeof(float));

    if (x->amplitudes == NULL || x->durations == NULL) {
        pd_error(x, "dynstoch~ • Cannot allocate memory for this object");
        return NULL;
    }

    x->freq = INITIAL_FREQ;
    x->min_freq = MINIMUM_FREQ;
    x->max_freq = MAXIMUM_FREQ;
    x->amplitude_deviation = DEFAULT_AMP_DEV;
    x->duration_deviation = DEFAULT_DUR_DEV;
    x->first_time = 1;

    /* Print message to Max window */
    post("dynstoch~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void dynstoch_free(t_dynstoch* x)
{

    /* Free allocated dynamic memory */
    free(x->amplitudes);
    free(x->durations);

    /* Print message to Max window */
    post("dynstoch~ • Memory was freed");
}


/* The object-specific methods
 * ************************************************/
void dynstoch_ampdev(t_dynstoch* x, float ampdev)
{
    if (ampdev < MINIMUM_AMP_DEV) {
        ampdev = MINIMUM_AMP_DEV;
        pd_error(
            x,
            "dynstoch~ • Invalid attribute: Amplitude deviation set to %.4f",
            MINIMUM_AMP_DEV);
    }
    if (ampdev > MAXIMUM_AMP_DEV) {
        ampdev = MAXIMUM_AMP_DEV;
        pd_error(
            x,
            "dynstoch~ • Invalid attribute: Amplitude deviation set to %.4f",
            MAXIMUM_AMP_DEV);
    }
    x->amplitude_deviation = ampdev;
}

void dynstoch_durdev(t_dynstoch* x, float durdev)
{
    if (durdev < MINIMUM_DUR_DEV) {
        durdev = MINIMUM_DUR_DEV;
        pd_error(
            x, "dynstoch~ • Invalid attribute: Duration deviation set to %.4f",
            MINIMUM_DUR_DEV);
    }
    x->duration_deviation = durdev;
}

void dynstoch_setfreq(t_dynstoch* x, float new_freq)
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

void dynstoch_freqrange(t_dynstoch* x, float min_freq, float max_freq)
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

void dynstoch_initwave(t_dynstoch* x)
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

void dynstoch_recalculate(t_dynstoch* x)
{
    float amplitude_adjustment;
    float duration_adjustment;

    x->total_length = 0;
    for (int ii = 0; ii < x->num_points; ii++) {
        amplitude_adjustment = dynstoch_rand(-x->amplitude_deviation,
                                             x->amplitude_deviation);
        duration_adjustment = dynstoch_rand(-x->duration_deviation,
                                            x->duration_deviation);

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
    if (x->total_length > x->max_duration) {
        difference = x->total_length - x->max_duration;
        x->total_length = x->max_duration;
        for (int ii = 0; ii < difference; ii++) {
            if (x->durations[ii % x->num_points] > 1) {
                x->durations[ii % x->num_points] -= 1;
            }
        }
    } else if (x->total_length < x->min_duration) {
        difference = x->min_duration - x->total_length;
        x->total_length = x->min_duration;
        for (int ii = 0; ii < difference; ii++) {
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


/* The 'DSP' method
 * ***********************************************************/
void dynstoch_dsp(t_dynstoch* x, t_signal** sp, short* count)
{
    if (sp[0]->s_sr == 0) {
        pd_error(x, "dynstoch~ • Sampling rate is equal to zero!");
        return;
    }

    /* Initialize state variables */
    x->sr = sp[0]->s_sr;

    /* Initialize waveform */
    dynstoch_freqrange(x, x->min_freq, x->max_freq);
    if (x->first_time) {
        dynstoch_initwave(x);
        x->first_time = 0;
    }

    /* Attach the object to the DSP chain */
    dsp_add(dynstoch_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[0]->s_n);

    /* Print message to Max window */
    post("dynstoch~ • Executing 32-bit perform routine");
}

/* The 'perform' routine
 * ******************************************************/
t_int* dynstoch_perform(t_int* w)
{
    /* Copy the object pointer */
    t_dynstoch* x = (t_dynstoch*)w[OBJECT];

    /* Copy signal pointers */
    t_float* output = (t_float*)w[OUTPUT1];
    t_float* frequency = (t_float*)w[FREQUENCY];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Load state variables */
    int num_points = x->num_points;
    float* amplitudes = x->amplitudes;
    float* durations = x->durations;

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

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
