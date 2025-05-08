
#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

/* The global variables *******************************************************/
#define GAIN 0.1

/* The object structure *******************************************************/
typedef struct _mirror {
	t_pxobject obj;
} t_mirror;

/* The class pointer **********************************************************/
static t_class *mirror_class;

/* Function prototypes ********************************************************/
void mirror_dsp64(t_mirror* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void mirror_perform64(t_mirror* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

void *mirror_new(void);
void mirror_free(t_mirror *x);
void mirror_assist(t_mirror *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
	/* Initialize the class */
	mirror_class = class_new("mirror~",
							 (method)mirror_new,
							 (method)mirror_free,
							 sizeof(t_mirror), 0, 0);
	
	/* Bind the DSP method, which is called when the DACs are turned on */
	class_addmethod(mirror_class, (method)mirror_dsp64, "dsp64", A_CANT, 0);
	
	/* Bind the assist method, which is called on mouse-overs to inlets and outlets */
	class_addmethod(mirror_class, (method)mirror_assist, "assist", A_CANT, 0);
	
	/* Add standard Max methods to the class */
	class_dspinit(mirror_class);
	
	/* Register the class with Max */
	class_register(CLASS_BOX, mirror_class);
	
	/* Print message to Max window */
	object_post(NULL, "mirror~ • External was loaded");
	
	/* Return with no error */
	return 0;
}

/* The inlets/outlets indexes *************************************************/
enum INLETS {INPUT1, NUM_INLETS};
enum OUTLETS {OUTPUT1, NUM_OUTLETS};

/* The 'new instance' routine *************************************************/
void *mirror_new(void)
{
	/* Instantiate a new object */
	t_mirror *x = (t_mirror *)object_alloc(mirror_class);
	
	/* Create signal inlets */
	dsp_setup((t_pxobject *)x, NUM_INLETS);
	
	/* Create signal outlets */
	outlet_new((t_object *)x, "signal");
	
	/* Print message to Max window */
	object_post((t_object *)x, "Object was created");
	
	/* Return a pointer to the new object */
	return x;
}

/* The 'free instance' routine ************************************************/
void mirror_free(t_mirror *x)
{
	dsp_free((t_pxobject *)x);
	
	/* Print message to Max window */
	object_post((t_object *)x, "Memory was freed");
}

/* The 'assist' method ********************************************************/
void mirror_assist(t_mirror *x, void *b, long msg, long arg, char *dst)
{
	/* Document inlet functions */
	if (msg == ASSIST_INLET) {
		switch (arg) {
			case INPUT1: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Input"); break;
		}
	}
	
	/* Document outlet functions */
	else if (msg == ASSIST_OUTLET) {
		switch (arg) {
			case OUTPUT1: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Output"); break;
		}
	}
}


/* The 'DSP/perform' arguments list *******************************************/
enum DSP {PERFORM, OBJECT, INPUT_VECTOR, OUTPUT_VECTOR, VECTOR_SIZE, NEXT};


void mirror_dsp64(t_mirror* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    post("mirror~ • Executing 64-bit perform routine");

    object_method(dsp64, gensym("dsp_add64"), x, mirror_perform64, 0, NULL);
}

void mirror_perform64(t_mirror* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* inL = ins[0];
    t_double* outL = outs[0];
    int n = sampleframes;

    while (n--) {
        *outL++ = GAIN * *inL++;
    }
}


