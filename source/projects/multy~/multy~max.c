#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "z_sampletype.h"

/* The object structure *******************************************************/
typedef struct _multy {
    t_pxobject obj;
} t_multy;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT1, I_INPUT2, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT1, INPUT2, OUTPUT,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *multy_class;

/* Function prototypes ********************************************************/
void *multy_common_new(t_multy *x, short argc, t_atom *argv);
void multy_free(t_multy *x);

void multy_dsp64(t_multy *x, t_object *dsp64,
                   short *count,
                   double samplerate,
                   long maxvectorsize,
                   long flags);
void multy_perform64(t_multy *x, t_object *dsp64,
                       double **ins, long numins,
                       double **outs, long numouts,
                       long vectorsize,
                       long flags,
                       void *userparams);

/* Function prototypes ********************************************************/
void *multy_new(t_symbol *s, short argc, t_atom *argv);

void multy_float(t_multy *x, double farg);
void multy_assist(t_multy *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    multy_class = class_new("multy~",
                              (method)multy_new,
                              (method)multy_free,
                              sizeof(t_multy), 0, A_GIMME, 0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    // class_addmethod(multy_class, (method)multy_dsp, "dsp", A_CANT, 0);
    class_addmethod(multy_class, (method)multy_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(multy_class, (method)multy_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and outlets */
    class_addmethod(multy_class, (method)multy_assist, "assist", A_CANT, 0);

    /* Add standard Max methods to the class */
    class_dspinit(multy_class);

    /* Register the class with Max */
    class_register(CLASS_BOX, multy_class);

    /* Print message to Max window */
    object_post(NULL, "multy~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine *************************************************/
void *multy_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_multy *x = (t_multy *)object_alloc(multy_class);

    return multy_common_new(x, argc, argv);
}

/******************************************************************************/

/* The 'float' method *********************************************************/
void multy_float(t_multy *x, double farg)
{
    // nothing
}

/* The 'assist' method ********************************************************/
void multy_assist(t_multy *x, void *b, long msg, long arg, char *dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
            case I_INPUT1: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Input 1"); break;
            case I_INPUT2: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Input 2"); break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
            case O_OUTPUT: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Output"); break;
        }
    }
}

/******************************************************************************/


/* The common 'new instance' routine ******************************************/
void *multy_common_new(t_multy *x, short argc, t_atom *argv)
{

    /* Create inlets */
    dsp_setup((t_pxobject *)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object *)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;

    /* Print message to Max window */
    post("multy~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void multy_free(t_multy *x)
{
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject *)x);

    /* Print message to Max window */
    post("multy~ • Memory was freed");
}

/* The 'DSP64' method *********************************************************/
void multy_dsp64(t_multy *x, t_object *dsp64,
                   short *count,
                   double samplerate,
                   long maxvectorsize,
                   long flags)
{
    /* Attach the object to the DSP chain */
    //dsp_add64(dsp64, (t_object *)x, (t_perfroutine64)multy_perform64, 0, NULL);
    object_method(dsp64, gensym("dsp_add64"), x, multy_perform64, 0, NULL);

    /* Print message to Max window */
    post("multy~ • Executing 64-bit perform routine");
}

/* The 'perform64' routine ****************************************************/
void multy_perform64(t_multy *x, t_object *dsp64,
                       double **inputs, long numinputs,
                       double **outputs, long numoutputs,
                       long vectorsize,
                       long flags,
                       void *userparams)
{
    /* Copy signal pointers */
    t_double *input1 = inputs[0];
    t_double *input2 = inputs[1];
    t_double *output = outputs[0];

    /* Copy the signal vector size */
    long n = vectorsize;

    /* Perform the DSP loop */
    while (n--) {
        *output++ = *input1++ * *input2++;
    }
}
