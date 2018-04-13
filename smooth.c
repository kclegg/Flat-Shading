/*  
    smooth.c
    Nate Robins, 1998

    Model viewer program.  Exercises the glm library.
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <GL/glut.h>
#include "gltb.h"
#include "glm.h"
#include "dirent32.h"

#pragma comment( linker, "/entry:\"mainCRTStartup\"" )  // set the entry point to be main()

#define DATA_DIR "data/"

char*      model_file = NULL;		/* name of the obect file */
GLuint     model_list = 0;		    /* display list for object */
GLMmodel*  model;			        /* glm model data structure */
GLfloat    scale;			        /* original scale factor */
GLfloat    smoothing_angle = 90.0;	/* smoothing angle */
GLfloat    weld_distance = 0.00001;	/* epsilon for welding vertices */
GLboolean  facet_normal = GL_FALSE;	/* draw with facet normal? */
GLboolean  bounding_box = GL_FALSE;	/* bounding box on? */
GLboolean  performance = GL_FALSE;	/* performance counter on? */
GLboolean  stats = GL_FALSE;		/* statistics on? */
GLuint     material_mode = 0;		/* 0=none, 1=color, 2=material */
GLint      entries = 0;			    /* entries in model menu */
GLdouble   pan_x = 0.0;
GLdouble   pan_y = 0.0;
GLdouble   pan_z = 0.0;
GLint 	   yvalue = 0;

#define CLK_TCK 1000

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>

float
elapsed(void)
{
    static long begin = 0;
    static long finish, difference;
    
#if defined(_WIN32)
    static struct timeb tb;
    ftime(&tb);
    finish = tb.time*1000+tb.millitm;
#else
    static struct tms tb;
    finish = times(&tb);
#endif
    
    difference = finish - begin;
    begin = finish;
    
    return (float)difference/(float)CLK_TCK;
}

void
shadowtext(int x, int y, char* s) 
{
    int lines;
    char* p;
    
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), 
        0, glutGet(GLUT_WINDOW_HEIGHT), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3ub(0, 0, 0);
    glRasterPos2i(x+1, y-1);
    for(p = s, lines = 0; *p; p++) {
        if (*p == '\n') {
            lines++;
            glRasterPos2i(x+1, y-1-(lines*18));
        }
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }
    glColor3ub(0, 128, 255);
    glRasterPos2i(x, y);
    for(p = s, lines = 0; *p; p++) {
        if (*p == '\n') {
            lines++;
            glRasterPos2i(x, y-(lines*18));
        }
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

void
lists(void)
{
    GLfloat ambient[] = { 0.2, 0.2, 0.2, 1.0 };
    GLfloat diffuse[] = { 0.8, 0.8, 0.8, 1.0 };
    GLfloat specular[] = { 0.0, 0.0, 0.0, 1.0 };
    GLfloat shininess = 65.0;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
    
    if (model_list)
        glDeleteLists(model_list, 1);
    
    /* generate a list */
    if (material_mode == 0) { 
        if (facet_normal)
            model_list = glmList(model, GLM_FLAT);
        else
            model_list = glmList(model, GLM_SMOOTH);
    } else if (material_mode == 1) {
        if (facet_normal)
            model_list = glmList(model, GLM_FLAT | GLM_COLOR);
        else
            model_list = glmList(model, GLM_SMOOTH | GLM_COLOR);
    } else if (material_mode == 2) {
        if (facet_normal)
            model_list = glmList(model, GLM_FLAT | GLM_MATERIAL);
        else
            model_list = glmList(model, GLM_SMOOTH | GLM_MATERIAL);
    }
}

void
init(void)
{
    gltbInit(GLUT_LEFT_BUTTON);
    
    /* read in the model */
    model = glmReadOBJ(model_file);
    scale = glmUnitize(model);
    glmFacetNormals(model);
    glmVertexNormals(model, smoothing_angle);
    
    if (model->nummaterials > 0)
        material_mode = 2;
    
    /* create new display lists */
    lists();
    
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    
    glEnable(GL_DEPTH_TEST);
    
    glEnable(GL_CULL_FACE);
}

void
reshape(int width, int height)
{
    gltbReshape(width, height);
    
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (GLfloat)height / (GLfloat)width, 1.0, 128.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -3.0);
}


///////////////////////////////////////////////////////////////////////////////////////////

#define NUM_FRAMES 5
void
display(void)
{
    static char s[256], t[32];
    static char* p;
    static int frames = 0;
    
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glPushMatrix();
    
    glTranslatef(pan_x, pan_y, 0.0);
    
    gltbMatrix();
    
#if 0   /* glmDraw() performance test */
    if (material_mode == 0) { 
        if (facet_normal)
            glmDraw(model, GLM_FLAT);
        else
            glmDraw(model, GLM_SMOOTH);
    } else if (material_mode == 1) {
        if (facet_normal)
            glmDraw(model, GLM_FLAT | GLM_COLOR);
        else
            glmDraw(model, GLM_SMOOTH | GLM_COLOR);
    } else if (material_mode == 2) {
        if (facet_normal)
            glmDraw(model, GLM_FLAT | GLM_MATERIAL);
        else
            glmDraw(model, GLM_SMOOTH | GLM_MATERIAL);
    }
#else
    glCallList(model_list);
#endif
    
    glDisable(GL_LIGHTING);
    if (bounding_box) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glColor4f(1.0, 0.0, 0.0, 0.25);
        glutSolidCube(2.0);
        glDisable(GL_BLEND);
    }
    
    glPopMatrix();
    
    if (stats) {
        /* XXX - this could be done a _whole lot_ faster... */
        int height = glutGet(GLUT_WINDOW_HEIGHT);
        glColor3ub(0, 0, 0);
        sprintf(s, "%s\n%d vertices\n%d triangles\n%d normals\n"
            "%d texcoords\n%d groups\n%d materials",
            model->pathname, model->numvertices, model->numtriangles, 
            model->numnormals, model->numtexcoords, model->numgroups,
            model->nummaterials);
        shadowtext(5, height-(5+18*1), s);
    }
    
    /* spit out frame rate. */
    frames++;
    if (frames > NUM_FRAMES) {
        sprintf(t, "%g fps", frames/elapsed());
        frames = 0;
    }
    if (performance) {
        shadowtext(5, 5, t);
    }
    
    if(yvalue == 1){

	GLfloat rgbPixelsdata[512][512][3];
	int Hit[512][512];			// array of all of the (x,y) pixels not altered
	GLdouble zbuffer [512][512][1];		// for each pixel we have a z value for closest tri

	for(int k = 0; k < 512; k++)
	{
	    for(int l = 0; l < 512; l++)
	    {
		rgbPixelsdata[k][l][0] = 0.0f;
		rgbPixelsdata[k][l][1] = 0.0f;
		rgbPixelsdata[k][l][2] = 0.0f;
		
		Hit[k][l] = 0;
		zbuffer[k][l][0] = 100;
	    }
	}

	GLMgroup *loop1 = model->groups;
	GLuint numT = loop1->numtriangles;
	GLMmaterial material = model->materials[loop1->material];

	GLdouble V1coordinatex, V1coordinatey, V1coordinatez;	// Vertex 1
	GLdouble V2coordinatex, V2coordinatey, V2coordinatez;	// Vertex 2
	GLdouble V3coordinatex, V3coordinatey, V3coordinatez;	// Vertex 3

	double V1normalx, V1normaly, V1normalz;
	double V2normalx, V2normaly, V2normalz;
	double V3normalx, V3normaly, V3normalz;

	GLdouble xparm = 0;
	GLdouble yparm = 0;
	GLdouble zparm = 0;

	GLdouble modelParameter[4*4];
	GLdouble projParameter[4*4];
	GLint viewParameter[4];
	    
	glGetDoublev(GL_MODELVIEW_MATRIX, modelParameter);
        glGetDoublev(GL_PROJECTION_MATRIX, projParameter);
        glGetIntegerv(GL_VIEWPORT, viewParameter);

	int whileLoop = 0;
	
	while(whileLoop == 0)
	{ 

	    // For each triangle in the group get the vertex data and rasterize each triangle.

	    for(int loop2 = 0; loop2 < numT; loop2++)
	    {
		    
	        GLMtriangle triangleData = model->triangles[loop1->triangles[loop2]];
				
		V1normalx = model->normals[3 * triangleData.nindices[0] + 0];
		V1normaly = model->normals[3 * triangleData.nindices[0] + 1];
		V1normalz = model->normals[3 * triangleData.nindices[0] + 2]; 

		xparm = model->vertices[3 * triangleData.vindices[0] + 0];
		yparm = model->vertices[3 * triangleData.vindices[0] + 1];
		zparm = model->vertices[3 * triangleData.vindices[0] + 2];

	        gluProject(xparm, yparm, zparm, modelParameter, projParameter, viewParameter, &V1coordinatex, &V1coordinatey, &V1coordinatez);
				
		V2normalx = model->normals[3 * triangleData.nindices[1] + 0];
		V2normaly = model->normals[3 * triangleData.nindices[1] + 1];
		V2normalz = model->normals[3 * triangleData.nindices[1] + 2];

		xparm = model->vertices[3 * triangleData.vindices[1] + 0];
		yparm = model->vertices[3 * triangleData.vindices[1] + 1];
		zparm = model->vertices[3 * triangleData.vindices[1] + 2];

	        gluProject(xparm, yparm, zparm, modelParameter, projParameter, viewParameter, &V2coordinatex, &V2coordinatey, &V2coordinatez);

		V3normalx = model->normals[3 * triangleData.nindices[2] + 0];
		V3normaly = model->normals[3 * triangleData.nindices[2] + 1];
		V3normalz = model->normals[3 * triangleData.nindices[2] + 2];

		xparm = model->vertices[3 * triangleData.vindices[2] + 0];
		yparm = model->vertices[3 * triangleData.vindices[2] + 1];
		zparm = model->vertices[3 * triangleData.vindices[2] + 2];

		gluProject(xparm, yparm, zparm, modelParameter, projParameter, viewParameter, &V3coordinatex, &V3coordinatey, &V3coordinatez);
		
		// Now Rasterization

		// Gouraud Shading gets the color 
		// of each vertex (3 colors). Then
		// interpolates the triangle.
		
		GLdouble z = 0.0;
		if(V1coordinatez > V2coordinatez && V1coordinatez > V3coordinatez){z = V1coordinatez;}
		else if(V2coordinatez > V1coordinatez && V2coordinatez > V3coordinatez){z = V2coordinatez;}
		else {z = V3coordinatez;}
 
		int arg1,arg2,arg3;
	    	double normalx, normaly, normalz;
	
		for(int y1 = 0; y1 < 512; y1++)
		{
		    for(int x1 = 0; x1 < 512; x1++)
		    {
			GLdouble sign1 = (x1 - V2coordinatex) * (V1coordinatey - V2coordinatey) - (V1coordinatex - V2coordinatex) * (y1 - V2coordinatey);
			GLdouble sign2 = (x1 - V3coordinatex) * (V2coordinatey - V3coordinatey) - (V2coordinatex - V3coordinatex) * (y1 - V3coordinatey);
			GLdouble sign3 = (x1 - V1coordinatex) * (V3coordinatey - V1coordinatey) - (V3coordinatex - V1coordinatex) * (y1 - V1coordinatey);

			if(sign1 < 0){arg1 = 1;}
			else{arg1 = 0;}

			if(sign2 < 0){arg2 = 1;}
			else{arg2 = 0;}

			if(sign3 < 0){arg3 = 1;}
			else{arg3 = 0;}

			// Before going into if statement check 
			// that z' < current z' to confirm that
			// the pixel we are overriding is from a
			// triangle that is closer to the camera
			// than the previous one. 
			
			// Add an AND statement to the current IF clause
			

			if(((arg1 == arg2) && (arg2 == arg3)) && zbuffer[y1][x1][0] > z)
			{
			    // Now using our normal vertices do flat shading !!.
			    // This will give us color values that we then, set below.

			    zbuffer[y1][x1][0] = z;
			    Hit[y1][x1] = 1;	// Tells us this pixels color has been changed

			    normalx = (V1normalx + V2normalx + V3normalx) / 3;
			    normaly = (V1normaly + V2normaly + V3normaly) / 3;
			    normalz = (V1normalz + V2normalz + V3normalz) / 3;
			    
			    normalx = (normalx) / (sqrt((normalx*normalx) + (normaly*normaly) + (normalz*normalz)));
			    normaly = (normaly) / (sqrt((normalx*normalx) + (normaly*normaly) + (normalz*normalz)));
			    normalz = (normalz) / (sqrt((normalx*normalx) + (normaly*normaly) + (normalz*normalz)));

			    float ambientR = 0.9*material.ambient[0];
			    float ambientG = 0.9*material.ambient[1];
			    float ambientB = 0.9*material.ambient[2];
			    float coeff = (normalx * -1.0f);

			    if(coeff < 0.0){coeff = 0.0;}

			    float diffuseR = (coeff)*material.diffuse[0];
			    float diffuseG = (coeff)*material.diffuse[1];
			    float diffuseB = (coeff)*material.diffuse[2];
			
			    coeff = (normalx * ((-1.0f) / (sqrt((1.0f*1.0f)))));
			    
			    if(coeff < 0.0){coeff = 0.0;}

			    coeff = 0.9*pow(coeff, material.shininess);

			    float specularR = (coeff)*material.specular[0];
			    float specularG = (coeff)*material.specular[1];
			    float specularB = (coeff)*material.specular[2];
			    
			    rgbPixelsdata[y1][x1][0] = (ambientR + diffuseR + specularR);
			    rgbPixelsdata[y1][x1][1] = (ambientG + diffuseG + specularG);
			    rgbPixelsdata[y1][x1][2] = (ambientB + diffuseB + specularB);
			}
		    }
		}

	    }

	    if(loop1->next == NULL || !(loop1->next))
	    {
		whileLoop = 1;
	    }
	    else
	    {
		loop1 = loop1->next;				// go to the next group
		numT = loop1->numtriangles;			// get the new groups number of triangles
		material = model->materials[loop1->material]; 	// get the next material of new group
	    }
	}
	
	for(int y1 = 0; y1 < 512; y1++)
	{
	    for(int x1 = 0; x1 < 512; x1++)
	    {
		if(Hit[y1][x1] == 0)	// If pixel's color has not been changed set to white
		{
		    rgbPixelsdata[y1][x1][0] = 1.0f;	// Alternative is to use (GLubyte) 0-255
		    rgbPixelsdata[y1][x1][1] = 1.0f;
		    rgbPixelsdata[y1][x1][2] = 1.0f;			
		}
	    }
	}

	glDrawPixels(512, 512, GL_RGB, GL_FLOAT, rgbPixelsdata);
	yvalue = 0;
    }

    glutSwapBuffers();
    glEnable(GL_LIGHTING);
}


/////////////////////////////////////////////////////////////////////////////////////////////



void
keyboard(unsigned char key, int x, int y)
{
    GLint params[2];
    
    switch (key) {
    case 'h':
        printf("help\n\n");
        printf("w         -  Toggle wireframe/filled\n");
        printf("c         -  Toggle culling\n");
        printf("n         -  Toggle facet/smooth normal\n");
        printf("b         -  Toggle bounding box\n");
        printf("r         -  Reverse polygon winding\n");
        printf("m         -  Toggle color/material/none mode\n");
        printf("p         -  Toggle performance indicator\n");
        printf("s/S       -  Scale model smaller/larger\n");
        printf("t         -  Show model stats\n");
        printf("o         -  Weld vertices in model\n");
        printf("+/-       -  Increase/decrease smoothing angle\n");
        printf("W         -  Write model to file (out.obj)\n");
        printf("q/escape  -  Quit\n\n");
        break;
        
    case 't':
        stats = !stats;
        break;
        
    case 'p':
        performance = !performance;
        break;
        
    case 'm':
        material_mode++;
        if (material_mode > 2)
            material_mode = 0;
        printf("material_mode = %d\n", material_mode);
        lists();
        break;
        
    case 'd':
        glmDelete(model);
        init();
        lists();
        break;
        
    case 'w':
        glGetIntegerv(GL_POLYGON_MODE, params);
        if (params[0] == GL_FILL)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
        
    case 'c':
        if (glIsEnabled(GL_CULL_FACE))
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
        break;
        
    case 'b':
        bounding_box = !bounding_box;
        break;
        
    case 'n':
        facet_normal = !facet_normal;
        lists();
        break;
        
    case 'r':
        glmReverseWinding(model);
        lists();
        break;
        
    case 's':
        glmScale(model, 0.8);
        lists();
        break;
        
    case 'S':
        glmScale(model, 1.25);
        lists();
        break;
        
    case 'o':
        //printf("Welded %d\n", glmWeld(model, weld_distance));
        glmVertexNormals(model, smoothing_angle);
        lists();
        break;
        
    case 'O':
        weld_distance += 0.01;
        printf("Weld distance: %.2f\n", weld_distance);
        glmWeld(model, weld_distance);
        glmFacetNormals(model);
        glmVertexNormals(model, smoothing_angle);
        lists();
        break;
        
    case '-':
        smoothing_angle -= 1.0;
        printf("Smoothing angle: %.1f\n", smoothing_angle);
        glmVertexNormals(model, smoothing_angle);
        lists();
        break;
        
    case '+':
        smoothing_angle += 1.0;
        printf("Smoothing angle: %.1f\n", smoothing_angle);
        glmVertexNormals(model, smoothing_angle);
        lists();
        break;
        
    case 'W':
        glmScale(model, 1.0/scale);
        glmWriteOBJ(model, "out.obj", GLM_SMOOTH | GLM_MATERIAL);
        break;
    
    case 'y':
	yvalue = 1;
	break;

    case 'R':
        {
            GLuint i;
            GLfloat swap;
            for (i = 1; i <= model->numvertices; i++) {
                swap = model->vertices[3 * i + 1];
                model->vertices[3 * i + 1] = model->vertices[3 * i + 2];
                model->vertices[3 * i + 2] = -swap;
            }
            glmFacetNormals(model);
            lists();
            break;
        }
        
    case 27:
        exit(0);
        break;
    }
    
    glutPostRedisplay();
}

void
menu(int item)
{
    int i = 0;
    DIR* dirp;
    char* name;
    struct dirent* direntp;
    
    if (item > 0) {
        keyboard((unsigned char)item, 0, 0);
    } else {
        dirp = opendir(DATA_DIR);
        while ((direntp = readdir(dirp)) != NULL) {
            if (strstr(direntp->d_name, ".obj")) {
                i++;
                if (i == -item)
                    break;
            }
        }
        if (!direntp)
            return;
        name = (char*)malloc(strlen(direntp->d_name) + strlen(DATA_DIR) + 1);
        strcpy(name, DATA_DIR);
        strcat(name, direntp->d_name);
        model = glmReadOBJ(name);
        scale = glmUnitize(model);
        glmFacetNormals(model);
        glmVertexNormals(model, smoothing_angle);
        
        if (model->nummaterials > 0)
            material_mode = 2;
        else
            material_mode = 0;
        
        lists();
        free(name);
        
        glutPostRedisplay();
    }
}

static GLint      mouse_state;
static GLint      mouse_button;

void
mouse(int button, int state, int x, int y)
{
    GLdouble model[4*4];
    GLdouble proj[4*4];
    GLint view[4];
    
    /* fix for two-button mice -- left mouse + shift = middle mouse */
    if (button == GLUT_LEFT_BUTTON && glutGetModifiers() & GLUT_ACTIVE_SHIFT)
        button = GLUT_MIDDLE_BUTTON;
    
    gltbMouse(button, state, x, y);
    
    mouse_state = state;
    mouse_button = button;
    
    if (state == GLUT_DOWN && button == GLUT_MIDDLE_BUTTON) {
        glGetDoublev(GL_MODELVIEW_MATRIX, model);
        glGetDoublev(GL_PROJECTION_MATRIX, proj);
        glGetIntegerv(GL_VIEWPORT, view);
        gluProject((GLdouble)x, (GLdouble)y, 0.0,
            model, proj, view,
            &pan_x, &pan_y, &pan_z);
        gluUnProject((GLdouble)x, (GLdouble)y, pan_z,
            model, proj, view,
            &pan_x, &pan_y, &pan_z);
        pan_y = -pan_y;
    }
    
    glutPostRedisplay();
}

void
motion(int x, int y)
{
    GLdouble model[4*4];
    GLdouble proj[4*4];
    GLint view[4];
    
    gltbMotion(x, y);
    
    if (mouse_state == GLUT_DOWN && mouse_button == GLUT_MIDDLE_BUTTON) {
        glGetDoublev(GL_MODELVIEW_MATRIX, model);
        glGetDoublev(GL_PROJECTION_MATRIX, proj);
        glGetIntegerv(GL_VIEWPORT, view);
        gluProject((GLdouble)x, (GLdouble)y, 0.0,
            model, proj, view,
            &pan_x, &pan_y, &pan_z);
        gluUnProject((GLdouble)x, (GLdouble)y, pan_z,
            model, proj, view,
            &pan_x, &pan_y, &pan_z);
        pan_y = -pan_y;
    }
    
    glutPostRedisplay();
}

int
main(int argc, char** argv)
{
    int buffering = GLUT_DOUBLE;
    struct dirent* direntp;
    DIR* dirp;
    int models;
    
    glutInitWindowSize(512, 512);
    glutInit(&argc, argv);
    
    while (--argc) {
        if (strcmp(argv[argc], "-sb") == 0)
            buffering = GLUT_SINGLE;
        else
            model_file = argv[argc];
    }
    
    if (!model_file) {
        model_file = "data/dolphins.obj";
    }
    
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | buffering);
    glutCreateWindow("Smooth");
    
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    
    models = glutCreateMenu(menu);
    dirp = opendir(DATA_DIR);
    if (!dirp) {
        fprintf(stderr, "%s: can't open data directory.\n", argv[0]);
    } else {
        while ((direntp = readdir(dirp)) != NULL) {
            if (strstr(direntp->d_name, ".obj")) {
                entries++;
                glutAddMenuEntry(direntp->d_name, -entries);
            }
        }
        closedir(dirp);
    }
    
    glutCreateMenu(menu);
    glutAddMenuEntry("Smooth", 0);
    glutAddMenuEntry("", 0);
    glutAddSubMenu("Models", models);
    glutAddMenuEntry("", 0);
    glutAddMenuEntry("[w]   Toggle wireframe/filled", 'w');
    glutAddMenuEntry("[c]   Toggle culling on/off", 'c');
    glutAddMenuEntry("[n]   Toggle face/smooth normals", 'n');
    glutAddMenuEntry("[b]   Toggle bounding box on/off", 'b');
    glutAddMenuEntry("[p]   Toggle frame rate on/off", 'p');
    glutAddMenuEntry("[t]   Toggle model statistics", 't');
    glutAddMenuEntry("[m]   Toggle color/material/none mode", 'm');
    glutAddMenuEntry("[r]   Reverse polygon winding", 'r');
    glutAddMenuEntry("[s]   Scale model smaller", 's');
    glutAddMenuEntry("[S]   Scale model larger", 'S');
    glutAddMenuEntry("[o]   Weld redundant vertices", 'o');
    glutAddMenuEntry("[+]   Increase smoothing angle", '+');
    glutAddMenuEntry("[-]   Decrease smoothing angle", '-');
    glutAddMenuEntry("[W]   Write model to file (out.obj)", 'W');
    glutAddMenuEntry("[y]   Switch to Rasterization Pipeline", 'y');
    glutAddMenuEntry("", 0);
    glutAddMenuEntry("[Esc] Quit", 27);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    
    init();
    
    glutMainLoop();
    return 0;
}
