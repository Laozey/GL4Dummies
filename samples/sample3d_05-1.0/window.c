/*!\file window.c
 * \brief g�om�tries lumi�re diffuse et transformations de base en
 * GL4Dummies + simulation de mobiles et gestion du picking des objets
 * + ombre port�e utilisant le shadow-mapping
 * \author Far�s BELHADJ, amsi@ai.univ-paris8.fr
 * \date March 10 2017 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <GL4D/gl4df.h>
#include <GL4D/gl4du.h>
#include <GL4D/gl4duw_SDL2.h>
#include "mobile.h"
/* Prototypes des fonctions statiques contenues dans ce fichier C */
static void init(void);
static void mouse(int button, int state, int x, int y);
static void motion(int x, int y);
static void draw(void);
static void quit(void);
/*!\brief dimensions de la fen�tre */
static int _windowWidth = 800, _windowHeight = 600;
/*!\brief identifiant du programme de coloriage GLSL */
static GLuint _shPID = 0;
/*!\brief identifiant du programme shadow map GLSL */
static GLuint _smPID = 0;
/*!\brief quelques objets g�om�triques */
static GLuint _sphere = 0, _quad = 0;
/*!\brief scale du plan */
static GLfloat _plan_s = 5.0f;
/*!\brief Framebuffer Object */
static GLuint _fbo = 0;
/*!\brief Texture recevant la couleur */
static GLuint _colorTex = 0;
/*!\brief Texture recevant la profondeur */
static GLuint _depthTex = 0;
/*!\brief Texture recevant les identifiants d'objets */
static GLuint _idTex = 0;
/*!\brief Texture recevant la shadow map */
static GLuint _smTex = 0;
/*!\brief nombre de mobiles cr��s dans la sc�ne */
static GLuint _nb_mobiles = 30;
/*!\brief identifiant du mobile s�lectionn� */
static int _picked_mobile = -1;
/*!\brief coordonn�es du mobile s�lectionn� */
static GLfloat _picked_mobile_coords[4] = {0};
/*!\brief copie CPU de la m�moire texture d'identifiants */
static GLfloat * _pixels = NULL;
/*!\brief position de la lumi�re, relative aux objets */
static GLfloat _lumpos[] = { 9, 3, 0, 1 };
#define SHADOW_MAP_SIDE 1024

/*!\brief La fonction principale cr�� la fen�tre d'affichage,
 * initialise GL et les donn�es, affecte les fonctions d'�v�nements et
 * lance la boucle principale d'affichage.*/
int main(int argc, char ** argv) {
  if(!gl4duwCreateWindow(argc, argv, "GL4D - Picking", 0, 0, _windowWidth, _windowHeight, GL4DW_SHOWN))
    return 1;
  init();
  atexit(quit);
  gl4duwMouseFunc(mouse);
  gl4duwMotionFunc(motion);
  gl4duwIdleFunc(mobileMove);
  gl4duwDisplayFunc(draw);
  gl4duwMainLoop();
  return 0;
}
/*!\brief initialise les param�tres OpenGL */
static void init(void) {
  glEnable(GL_DEPTH_TEST);
  _shPID  = gl4duCreateProgram("<vs>shaders/basic.vs", "<fs>shaders/basic.fs", NULL);
  _smPID  = gl4duCreateProgram("<vs>shaders/shadowMap.vs", "<fs>shaders/shadowMap.fs", NULL);
  gl4duGenMatrix(GL_FLOAT, "modelMatrix");
  gl4duGenMatrix(GL_FLOAT, "lightViewMatrix");
  gl4duGenMatrix(GL_FLOAT, "lightProjectionMatrix");
  gl4duGenMatrix(GL_FLOAT, "cameraViewMatrix");
  gl4duGenMatrix(GL_FLOAT, "cameraProjectionMatrix");
  gl4duGenMatrix(GL_FLOAT, "cameraPVMMatrix");

  glViewport(0, 0, _windowWidth, _windowHeight);
  gl4duBindMatrix("lightProjectionMatrix");
  gl4duLoadIdentityf();
  gl4duFrustumf(-3, 3, -3, 3, 1.5, 50.0);
  gl4duBindMatrix("cameraProjectionMatrix");
  gl4duLoadIdentityf();
  gl4duFrustumf(-0.5, 0.5, -0.5 * _windowHeight / _windowWidth, 0.5 * _windowHeight / _windowWidth, 1.0, 50.0);
  gl4duBindMatrix("modelMatrix");

  _sphere = gl4dgGenSpheref(30, 30);
  _quad = gl4dgGenQuadf();
  mobileInit(_nb_mobiles, _plan_s, _plan_s);

  /* Cr�ation et param�trage de la Texture de shadow map */
  glGenTextures(1, &_smTex);
  glBindTexture(GL_TEXTURE_2D, _smTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIDE, SHADOW_MAP_SIDE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

  /* Cr�ation et param�trage de la Texture recevant la couleur */
  glGenTextures(1, &_colorTex);
  glBindTexture(GL_TEXTURE_2D, _colorTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  /* Cr�ation et param�trage de la Texture recevant la profondeur */
  glGenTextures(1, &_depthTex);
  glBindTexture(GL_TEXTURE_2D, _depthTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, _windowWidth, _windowHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

  /* Cr�ation et param�trage de la Texture recevant les identifiants d'objets */
  glGenTextures(1, &_idTex);
  glBindTexture(GL_TEXTURE_2D, _idTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, _windowWidth, _windowHeight, 0, GL_RED, GL_UNSIGNED_INT, NULL);

  /* Cr�ation du Framebuffer Object */
  glGenFramebuffers(1, &_fbo);

  _pixels = malloc(_windowWidth * _windowHeight * sizeof *_pixels);
  assert(_pixels);
}
/*!\brief call-back au click (tous les boutons avec state � down (1) ou up (0)) */
static void mouse(int button, int state, int x, int y) {
  if(button == GL4D_BUTTON_LEFT) {
    y = _windowHeight - y;
    glBindTexture(GL_TEXTURE_2D, _idTex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, _pixels);
    if(x >= 0 && x < _windowWidth && y >=0 && y < _windowHeight)
      _picked_mobile = (int)((_nb_mobiles + 2.0f) * _pixels[y * _windowWidth + x]) - 3;
    if(_picked_mobile >= 0 && _picked_mobile < _nb_mobiles) {
      mobileSetFreeze(_picked_mobile, state);
      if(state) { /* au click, r�cup�ration de la coordonn�e espace-�cran du mobile */
	GLfloat m[16], tmpp[16], tmpm[16], * gl4dm;
	GLfloat mcoords[] = {0, 0, 0, 1};
	/* r�cup�ration des coordonn�es spaciales du mobile */
	mobileGetCoords(_picked_mobile, mcoords);
	/* copie de la matrice de projection dans tmpp */
	gl4duBindMatrix("cameraProjectionMatrix");
	gl4dm = gl4duGetMatrixData();
	memcpy(tmpp, gl4dm, sizeof tmpp);
	/* copie de la matrice de view dans tmpm */
	gl4duBindMatrix("cameraViewMatrix");
	gl4dm = gl4duGetMatrixData();
	memcpy(tmpm, gl4dm, sizeof tmpm);
	/* m est tmpp x tmpm */
	MMAT4XMAT4(m, tmpp, tmpm);
	/* modelisation et projection de la coordonn�e du mobile dans _picked_mobile_coords */
	MMAT4XVEC4(_picked_mobile_coords, m, mcoords);
	MVEC4WEIGHT(_picked_mobile_coords);
      }
    }
    if(!state)
      _picked_mobile = -1;
  }
}
/*!\brief call-back lors du drag souris */
static void motion(int x, int y) {
  if(_picked_mobile >= 0 && _picked_mobile < _nb_mobiles) {
    GLfloat m[16], tmpp[16], tmpm[16], * gl4dm;
    /* p est la coordonn�e de la souris entre -1 et +1 */
    GLfloat p[] = { 2.0f * x / (GLfloat)_windowWidth - 1.0f,
		    -(2.0f * y / (GLfloat)_windowHeight - 1.0f), 
		    0.0f, 1.0 }, ip[4];
    /* copie de la matrice de projection dans tmpp */
    gl4duBindMatrix("cameraProjectionMatrix");
    gl4dm = gl4duGetMatrixData();
    memcpy(tmpp, gl4dm, sizeof tmpp);
    /* copie de la matrice de view dans tmpm */
    gl4duBindMatrix("cameraViewMatrix");
    gl4dm = gl4duGetMatrixData();
    memcpy(tmpm, gl4dm, sizeof tmpm);
    /* m est tmpp x tmpm */
    MMAT4XMAT4(m, tmpp, tmpm);
    /* inversion de m */
    MMAT4INVERSE(m);
    /* ajout de la profondeur � l'�cran du mobile comme profondeur du click */
    p[2] = _picked_mobile_coords[2];
    /* ip est la tranform�e inverse de la coordonn�e du click (donc coordonn�e spaciale du click) */
    MMAT4XVEC4(ip, m, p);
    MVEC4WEIGHT(ip);
    /* affectation de ip comme nouvelle coordonn�e spaciale du mobile */
    mobileSetCoords(_picked_mobile, ip);
  }
}

/*!\brief la sc�ne est soit dessin�e du point de vu de la lumi�re (sm
 *  = GL_TRUE donc shadow map) soit dessin�e du point de vue de la
 *  cam�ra */
static inline void scene(GLboolean sm) {
  glEnable(GL_CULL_FACE);
  if(sm) {
    glCullFace(GL_FRONT);
    glUseProgram(_smPID);
    gl4duBindMatrix("lightViewMatrix");
    gl4duLoadIdentityf();
    gl4duLookAtf(_lumpos[0], _lumpos[1], _lumpos[2], 0, 2, 0, 0, 1, 0);
    gl4duBindMatrix("modelMatrix");
    gl4duLoadIdentityf();
  } else {
    GLfloat vert[] = {0, 1, 0, 1}, lp[4], *mat;
    glCullFace(GL_BACK);
    glUseProgram(_shPID);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _smTex);
    glUniform1i(glGetUniformLocation(_shPID, "smTex"), 0);
    gl4duBindMatrix("cameraViewMatrix");
    gl4duLoadIdentityf();
    gl4duLookAtf(0, 4, 18, 0, 2, 0, 0, 1, 0);
    /* lumi�re positionnelle */
    mat = gl4duGetMatrixData();
    MMAT4XVEC4(lp, mat, _lumpos);
    MVEC4WEIGHT(lp);
    glUniform4fv(glGetUniformLocation(_shPID, "lumpos"), 1, lp);
    gl4duBindMatrix("modelMatrix");
    gl4duLoadIdentityf();
    gl4duPushMatrix(); {
      gl4duTranslatef(_lumpos[0], _lumpos[1], _lumpos[2]);
      gl4duScalef(0.3f, 0.3f, 0.3f);
      gl4duSendMatrices();
    } gl4duPopMatrix();
    glUniform1i(glGetUniformLocation(_shPID, "id"), 2);
    glUniform1i(glGetUniformLocation(_shPID, "nb_mobiles"), _nb_mobiles);
    gl4dgDraw(_sphere);
    glUniform4fv(glGetUniformLocation(_shPID, "couleur"), 1, vert);
    glUniform1i(glGetUniformLocation(_shPID, "id"), 1);
  }
  gl4duPushMatrix(); {
    gl4duRotatef(-90, 1, 0, 0);
    gl4duScalef(_plan_s, _plan_s, _plan_s);
    gl4duSendMatrices();
  } gl4duPopMatrix();
  gl4dgDraw(_quad);
  gl4duSendMatrices();
  mobileDraw(_sphere);
}

/*!\brief dessine dans le contexte OpenGL actif. */
static void draw(void) {
  GLenum renderings[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
  glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
  /* d�sactiver le rendu de couleur et ne laisser que le depth, dans _smTex */
  glDrawBuffer(GL_NONE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,    GL_TEXTURE_2D, 0, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _smTex, 0);
  /* viewport de la shadow map et dessin de la sc�ne du point de vue de la lumi�re */
  glViewport(0, 0, SHADOW_MAP_SIDE, SHADOW_MAP_SIDE);
  glClear(GL_DEPTH_BUFFER_BIT);
  scene(GL_TRUE);
/*   glDrawBuffers(1, &renderings[0]); */
/*   glBindTexture(GL_TEXTURE_2D, _smTex); */
/*   glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, _pixels); */
/*   glBindTexture(GL_TEXTURE_2D, _colorTex); */
/*   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _windowWidth, _windowHeight, 0, GL_RED, GL_FLOAT, _pixels); */
/*   gl4dfConvTex2Frame(_colorTex); */
  //return;
  /* param�trer le fbo pour 2 rendus couleurs + un (autre) depth */
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _idTex, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
  glViewport(0, 0, 256, 256);//_windowWidth, _windowHeight);
  /* un seul rendu GL_COLOR_ATTACHMENT1 + effacement 0 */
  glDrawBuffers(1, &renderings[1]);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  /* un seul rendu GL_COLOR_ATTACHMENT0 + effacement couleur et depth */
  glDrawBuffers(1, renderings);
  glClearColor(1.0f, 0.7f, 0.7f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  /* deux rendus GL_COLOR_ATTACHMENT0 et GL_COLOR_ATTACHMENT1 */
  glDrawBuffers(2, renderings);

  scene(GL_FALSE);

  /* copie du fbo � l'�cran */
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, 256, 256, 0, 0, _windowWidth, _windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBlitFramebuffer(0, 0, _windowWidth, _windowHeight, 0, 0, _windowWidth, _windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}
/*!\brief appel�e au moment de sortir du programme (atexit), lib�re les �l�ments utilis�s */
static void quit(void) {
  if(_fbo) {
    glDeleteTextures(1, &_colorTex);
    glDeleteTextures(1, &_depthTex);
    glDeleteTextures(1, &_idTex);
    glDeleteTextures(1, &_smTex);
    glDeleteFramebuffers(1, &_fbo);
    _fbo = 0;
  }
  if(_pixels) {
    free(_pixels);
    _pixels = NULL;
  }
  gl4duClean(GL4DU_ALL);
}
