#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

#include <math.h>
#include <stdlib.h>

/* The object structure *******************************************************/
typedef struct _moogvcf {
    t_pxobject obj;

    double onedsr;
    double xnm1;
    double y1nm1;
    double y2nm1;
    double y3nm1;
    double y1n;
    double y2n;
    double y3n;
    double y4n;
} t_moogvcf;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT, I_FREQUENCY, I_RESONANCE, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT1, FREQUENCY, RESONANCE, OUTPUT1,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *moogvcf_class;

/* Function prototypes ********************************************************/
void *moogvcf_common_new(t_moogvcf *x, short argc, t_atom *argv);
void moogvcf_free(t_moogvcf *x);
void moogvcf_dsp64(t_moogvcf* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void moogvcf_perform64(t_moogvcf* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

void *moogvcf_new(t_symbol *s, short argc, t_atom *argv);
void moogvcf_float(t_moogvcf *x, double farg);
void moogvcf_assist(t_moogvcf *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    moogvcf_class = class_new("moogvcf~",
                              (method)moogvcf_new,
                              (method)moogvcf_free,
                              sizeof(t_moogvcf), 0, A_GIMME, 0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(moogvcf_class, (method)moogvcf_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(moogvcf_class, (method)moogvcf_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and outlets */
    class_addmethod(moogvcf_class, (method)moogvcf_assist, "assist", A_CANT, 0);

    /* Add standard Max methods to the class */
    class_dspinit(moogvcf_class);

    /* Register the class with Max */
    class_register(CLASS_BOX, moogvcf_class);

    /* Print message to Max window */
    object_post(NULL, "moogvcf~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine *************************************************/
void *moogvcf_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_moogvcf *x = (t_moogvcf *)object_alloc(moogvcf_class);

    return moogvcf_common_new(x, argc, argv);
}

/******************************************************************************/

/* The 'float' method *********************************************************/
void moogvcf_float(t_moogvcf *x, double farg)
{
    // nothing
}

/* The 'assist' method ********************************************************/
void moogvcf_assist(t_moogvcf *x, void *b, long msg, long arg, char *dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
            case I_INPUT: sprintf(dst, "(signal) Input"); break;
            case I_FREQUENCY: sprintf(dst, "(signal) Cutoff frequency"); break;
            case I_RESONANCE: sprintf(dst, "(signal) Resonance"); break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
            case O_OUTPUT: sprintf(dst, "(signal) Output"); break;
        }
    }
}

/******************************************************************************/

/* The common 'new instance' routine ******************************************/
void *moogvcf_common_new(t_moogvcf *x, short argc, t_atom *argv)
{
    /* Create inlets */
    dsp_setup((t_pxobject *)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object *)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;

    /* Print message to Max window */
    post("moogvcf~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void moogvcf_free(t_moogvcf *x)
{

    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject *)x);

    /* Free allocated dynamic memory */
    // nothing

    /* Print message to Max window */
    post("moogvcf~ • Memory was freed");
}

/******************************************************************************/

void moogvcf_dsp64(t_moogvcf* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    if (samplerate == 0) {
        error("moogvcf~ • Sampling rate is equal to zero!");
        return;
    }

    /* Initialize state variables */
    x->onedsr = 1 / samplerate;
    x->xnm1 = 0.0;
    x->y1nm1 = 0.0;
    x->y2nm1 = 0.0;
    x->y3nm1 = 0.0;
    x->y1n = 0.0;
    x->y2n = 0.0;
    x->y3n = 0.0;
    x->y4n = 0.0;

    object_method(dsp64, gensym("dsp_add64"), x, moogvcf_perform64, 0, NULL);
}

void moogvcf_perform64(t_moogvcf* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* input = ins[0];
    t_double* frequency = ins[1];
    t_double* resonance = ins[2];
    t_double* output = outs[0];
    int n = sampleframes;

    /* Load state variables */
    double onedsr = x->onedsr;
    double xnm1 = x->xnm1;
    double y1nm1 = x->y1nm1;
    double y2nm1 = x->y2nm1;
    double y3nm1 = x->y3nm1;
    double y1n = x->y1n;
    double y2n = x->y2n;
    double y3n = x->y3n;
    double y4n = x->y4n;

    /* Perform the DSP loop */
    double freq_factor = 1.78179 * onedsr;
    double frequency_normalized = 0.0;
    double kp = 0.0;
    double pp1d2 = 0.0;
    double scale = 0.0;

    double k = 0.0;
    double xn = 0.0;

    for (int ii = 0; ii < n; ii++) {
        // normalized frequency from 0 to nyquist
        frequency_normalized = *frequency * freq_factor;

        // empirical tunning
        kp = - 1.0
             + 3.6 * frequency_normalized
             - 1.6 * frequency_normalized * frequency_normalized;

        // timesaver
        pp1d2 = (kp + 1.0) * 0.5;

        // scaling factor
        scale = exp((1.0 - pp1d2) * 1.386249);

        // inverted feedback for corner peaking
        k = *resonance++ * scale;
        xn = *input++ - (k * y4n);

        // four cascade onepole filters (bilinear transform)
        y1n = (xn  + xnm1 ) * pp1d2 - (kp * y1n);
        y2n = (y1n + y1nm1) * pp1d2 - (kp * y2n);
        y3n = (y2n + y2nm1) * pp1d2 - (kp * y3n);
        y4n = (y3n + y3nm1) * pp1d2 - (kp * y4n);

        // update coefficients
        xnm1 = xn;
        y1nm1 = y1n;
        y2nm1 = y2n;
        y3nm1 = y3n;

        // clipper band limited sigmoid
        *output++ = y4n - (y4n * y4n * y4n) / 6.0;
    }

    /* Update state variables */
    x->xnm1 = xnm1;
    x->y1nm1 = y1nm1;
    x->y2nm1 = y2nm1;
    x->y3nm1 = y3nm1;
    x->y1n = y1n;
    x->y2n = y2n;
    x->y3n = y3n;
    x->y4n = y4n;
}
