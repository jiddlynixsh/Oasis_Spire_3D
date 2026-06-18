#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <string.h>

// ============ Particle System Configuration ============
const int MAX_PARTICLES = 250;
enum ParticleState {
    STATE_AIR,
    STATE_CONDENSING,
    STATE_FALLING
};

struct Particle {
    float x, y, z;
    float angle;
    float radius;
    float speed;
    float t_coil;
    int state;
    float alpha;
};

std::vector<Particle> particles;

// ============ Global Variables ============
float angleX = 15.0f;
float angleY = -30.0f;
int lastX, lastY;
float fanRotation = 0.0f;
float dropY = 0.0f;
float viewDistance = 13.0f;

bool isFaucetOpen = true;
float waterLevel = 0.0f;
float maxWaterLevel = 4.8f;
float valveAngle = 0.0f;

// Animation controls
bool autoRotate = false;
float autoRotateAngle = 0.0f;
float airParticleDensity = 0.2f;

// Window size management
int windowWidth = 1200;
int windowHeight = 600;
int uiWidth = 350;

// Smart Screen global variables
float currentHumidity = 82.5f;
float currentPurity = 99.9f;
float screenAnimTime = 0.0f;
float blinkTimer = 0.0f;
GLuint screenTextureId;

// ============ Shadow Control Variables ============
bool isRenderingShadow = false; // Flag to indicate if we are currently rendering the shadow pass

// Light source position
GLfloat g_lightPos[] = { 10.0f, 15.0f, 15.0f, 1.0f };

// Ground plane equation Ax + By + Cz + D = 0
// Ground is at Y = -0.3, so equation is 1*y + 0.3 = 0
GLfloat g_groundPlane[] = { 0.0f, 1.0f, 0.0f, 0.3f };

// ============ Bezier Control Points ============
GLfloat bladeCtrlPoints[4][4][3] = {
    {{-0.5, 0.0, 0.0}, {-0.2, 0.2, 0.0}, {0.2, 0.2, 0.0}, {0.5, 0.0, 0.0}},
    {{-0.6, 0.0, 1.0}, {-0.2, 0.4, 1.0}, {0.2, 0.4, 1.0}, {0.6, 0.0, 1.0}},
    {{-0.5, 0.0, 2.0}, {-0.2, 0.3, 2.0}, {0.2, 0.3, 2.0}, {0.5, 0.0, 2.0}},
    {{ 0.0, 0.0, 3.0}, { 0.0, 0.1, 3.0}, {0.0, 0.1, 3.0}, {0.0, 0.0, 3.0}}
};

// ============ Shadow Matrix Calculation ============
// Generates a projection matrix to flatten geometry onto a defined plane based on light position
void myShadowMatrix(GLfloat plane[4], GLfloat light[4]) {
    GLfloat dot = plane[0] * light[0] + plane[1] * light[1] + plane[2] * light[2] + plane[3] * light[3];
    GLfloat shadowMat[16];

    shadowMat[0] = dot - light[0] * plane[0];
    shadowMat[4] = 0.0f - light[0] * plane[1];
    shadowMat[8] = 0.0f - light[0] * plane[2];
    shadowMat[12] = 0.0f - light[0] * plane[3];

    shadowMat[1] = 0.0f - light[1] * plane[0];
    shadowMat[5] = dot - light[1] * plane[1];
    shadowMat[9] = 0.0f - light[1] * plane[2];
    shadowMat[13] = 0.0f - light[1] * plane[3];

    shadowMat[2] = 0.0f - light[2] * plane[0];
    shadowMat[6] = 0.0f - light[2] * plane[1];
    shadowMat[10] = dot - light[2] * plane[2];
    shadowMat[14] = 0.0f - light[2] * plane[3];

    shadowMat[3] = 0.0f - light[3] * plane[0];
    shadowMat[7] = 0.0f - light[3] * plane[1];
    shadowMat[11] = 0.0f - light[3] * plane[2];
    shadowMat[15] = dot - light[3] * plane[3];

    glMultMatrixf(shadowMat);
}

// ============ Draw Ground ============
void drawGround() {
    glDisable(GL_LIGHTING);
    glPushMatrix();
    // Floor is positioned at Y = -0.3
    glTranslatef(0.0f, -0.3f, 0.0f);

    // Floor color
    glColor3f(0.25f, 0.25f, 0.3f);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    float s = 20.0f;
    glVertex3f(-s, 0, -s);
    glVertex3f(-s, 0,  s);
    glVertex3f( s, 0,  s);
    glVertex3f( s, 0, -s);
    glEnd();

    glPopMatrix();
    glEnable(GL_LIGHTING);
}

// ============ Texture Loading ============
void loadScreenTexture() {
    int width, height, nrChannels;
    const char* filepath = "assets/monitor.png";
    unsigned char *data = stbi_load(filepath, &width, &height, &nrChannels, 0);

    if (data) {
        glGenTextures(1, &screenTextureId);
        glBindTexture(GL_TEXTURE_2D, screenTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GLint format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    } else {
        // Fallback texture generation if file is missing
        unsigned char checkImage[64][64][3];
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                checkImage[i][j][0] = 100; checkImage[i][j][1] = 0; checkImage[i][j][2] = 100;
            }
        }
        glGenTextures(1, &screenTextureId);
        glBindTexture(GL_TEXTURE_2D, screenTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, checkImage);
    }
}

// ============ Helper Functions ============
float randomFloat(float min, float max) {
    return min + (float)(rand()) / (float)(RAND_MAX / (max - min));
}

void renderStrokeString(float x, float y, float z, float scale, void *font, char *string) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(scale, scale, scale);
    for (char* c = string; *c != '\0'; c++) {
        glutStrokeCharacter(font, *c);
    }
    glPopMatrix();
}

void setMaterial(GLfloat* amb, GLfloat* diff, GLfloat* spec, GLfloat shininess) {
    // 1. Shadow Logic: If currently rendering a shadow, force translucent black
    if (isRenderingShadow) {
        glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
        return;
    }

    // 2. Apply material properties directly
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
}

void drawSolidFrustum(float baseRadius, float topRadius, float height) {
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    GLUquadric *q = gluNewQuadric();
    gluCylinder(q, baseRadius, topRadius, height, 40, 1);
    glPushMatrix();
    glRotatef(180, 1, 0, 0);
    gluDisk(q, 0.0, baseRadius, 40, 1);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(q, 0.0, topRadius, 40, 1);
    glPopMatrix();
    gluDeleteQuadric(q);
    glPopMatrix();
}

// ============ Initialization ============
void initParticles() {
    particles.resize(MAX_PARTICLES);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].state = STATE_AIR;
        particles[i].angle = randomFloat(0.0f, 6.28f);
        particles[i].radius = randomFloat(1.0f, 3.5f);
        particles[i].y = randomFloat(5.5f, 7.5f);
        particles[i].speed = randomFloat(0.05f, 0.1f);
        particles[i].alpha = randomFloat(0.3f, 0.6f);
        particles[i].x = particles[i].radius * cos(particles[i].angle);
        particles[i].z = particles[i].radius * sin(particles[i].angle);
    }
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glDisable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_AUTO_NORMAL);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    GLfloat light_ambient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    GLfloat light_diff[] = { 1.0f, 0.98f, 0.95f, 1.0f };
    GLfloat light_spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, g_lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_spec);

    glEnable(GL_LIGHT1);
    GLfloat light1_pos[] = { -10.0f, 5.0f, -5.0f, 1.0f };
    GLfloat light1_diff[] = { 0.3f, 0.3f, 0.4f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diff);

    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);

    glMap2f(GL_MAP2_VERTEX_3, 0, 1, 3, 4, 0, 1, 12, 4, &bladeCtrlPoints[0][0][0]);
    glEnable(GL_MAP2_VERTEX_3);

    srand(0);
    initParticles();
    loadScreenTexture();
}

// ============ 3D Object Rendering ============
void drawDisplayBase() {
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.0f);

    // Champagne color material
    // Since shadows project to Y=-0.3, they won't overlap this base
    GLfloat base_amb[] = { 0.2f, 0.15f, 0.1f, 1.0f };
    GLfloat base_diff[] = { 0.4f, 0.3f, 0.2f, 1.0f };
    GLfloat base_spec[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    setMaterial(base_amb, base_diff, base_spec, 10.0f);

    GLUquadric *quad = gluNewQuadric();
    glRotatef(-90, 1, 0, 0);

    // Base extends from -0.3 to 0.0
    glTranslatef(0.0f, 0.0f, -0.3f);
    gluCylinder(quad, 3.0, 3.0, 0.3, 40, 1);
    gluDisk(quad, 0.0, 3.0, 40, 1);

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.3f);
    gluDisk(quad, 0.0, 3.0, 40, 1); // Top surface

    // Decorative ring
    GLfloat metal_amb[] = { 0.2f, 0.2f, 0.25f, 1.0f };
    GLfloat metal_diff[] = { 0.7f, 0.7f, 0.75f, 1.0f };
    GLfloat metal_spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    setMaterial(metal_amb, metal_diff, metal_spec, 80.0f);
    glutSolidTorus(0.05, 2.9, 20, 40);
    glPopMatrix();

    glPopMatrix();
    gluDeleteQuadric(quad);
}



void drawSmartScreen() {
    if (isRenderingShadow) return;

    glPushMatrix();
    glTranslatef(0.0f, 2.2f, 0.68f);

    GLfloat metal_amb[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat metal_diff[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat metal_spec[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    setMaterial(metal_amb, metal_diff, metal_spec, 30.0f);

    glPushMatrix();
    glScalef(0.4f, 0.3f, 0.1f);
    glutSolidCube(1.0);
    glPopMatrix();

    glTranslatef(0.0f, 0.0f, 0.1f);
    glRotatef(-10, 1, 0, 0);

    glPushMatrix();
    glScalef(1.4f, 0.9f, 0.05f);
    GLfloat case_diff[] = { 0.1f, 0.1f, 0.12f, 1.0f };
    setMaterial(metal_amb, case_diff, metal_spec, 50.0f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.03f);

    GLfloat screen_emit[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glMaterialfv(GL_FRONT, GL_EMISSION, screen_emit);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screenTextureId);

    glColor3f(1,1,1);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.65f, -0.4f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.65f, -0.4f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.65f,  0.4f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.65f,  0.4f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    GLfloat text_color[] = { 0.0f, 1.0f, 1.0f, 1.0f };
    GLfloat warning_color[] = { 1.0f, 0.6f, 0.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_EMISSION, text_color);
    glLineWidth(1.5f);

    char buffer[32];
    sprintf(buffer, "HUM: %.1f%%", currentHumidity);
    renderStrokeString(-0.58f, 0.15f, 0.01f, 0.0011f, GLUT_STROKE_ROMAN, buffer);

    sprintf(buffer, "H2O: %.1f%%", currentPurity);
    renderStrokeString(-0.58f, -0.05f, 0.01f, 0.0011f, GLUT_STROKE_ROMAN, buffer);

    if(isFaucetOpen) glMaterialfv(GL_FRONT, GL_EMISSION, warning_color);
    sprintf(buffer, "%s", isFaucetOpen ? "DISPENSING" : "STANDBY");
    float offset = isFaucetOpen ? -0.55f : -0.2f;
    renderStrokeString(offset, -0.32f, 0.01f, 0.0011f, GLUT_STROKE_MONO_ROMAN, buffer);

    glMaterialfv(GL_FRONT, GL_EMISSION, text_color);
    float barHeight = (waterLevel / maxWaterLevel) * 0.42f;

    glDisable(GL_LIGHTING);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0.45f, -0.08f, 0.01f);
    glVertex3f(0.55f, -0.08f, 0.01f);
    glVertex3f(0.55f,  0.36f, 0.01f);
    glVertex3f(0.45f,  0.36f, 0.01f);
    glEnd();

    glBegin(GL_QUADS);
    glVertex3f(0.46f, -0.07f, 0.01f);
    glVertex3f(0.54f, -0.07f, 0.01f);
    glVertex3f(0.54f, -0.07f + barHeight, 0.01f);
    glVertex3f(0.46f, -0.07f + barHeight, 0.01f);
    glEnd();
    glEnable(GL_LIGHTING);

    float blink = (sin(blinkTimer) + 1.0f) * 0.5f;
    if (isFaucetOpen) glColor3f(1.0f * blink, 0.5f * blink, 0.0f);
    else glColor3f(0.0f, 1.0f * blink, 0.0f);

    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(0.5f, 0.3f, 0.02f);
    glutSolidSphere(0.03, 10, 10);
    glPopMatrix();
    glEnable(GL_LIGHTING);

    GLfloat no_emit[] = {0,0,0,1};
    glMaterialfv(GL_FRONT, GL_EMISSION, no_emit);

    glPopMatrix();
    glPopMatrix();
}

void drawLeaf() {
    glPushMatrix();
    GLfloat mat_amb[] = { 0.8f, 0.8f, 0.75f, 1.0f };
    GLfloat mat_diff[] = { 0.92f, 0.92f, 0.88f, 1.0f };
    GLfloat mat_spec[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    setMaterial(mat_amb, mat_diff, mat_spec, 30.0f);

    for(int k=0; k<2; k++) {
        glPushMatrix();
        glTranslatef(0.0f, k * 0.12f, 0.0f);
        glScalef(1.0f - k*0.1f, 0.1f, 1.0f - k*0.05f);
        if(k==1) {
             GLfloat mat_amb_dark[] = { 0.7f, 0.7f, 0.65f, 1.0f };
             GLfloat mat_diff_dark[] = { 0.8f, 0.8f, 0.75f, 1.0f };
             setMaterial(mat_amb_dark, mat_diff_dark, mat_spec, 30.0f);
        }
        glScalef(1.2f, 0.6f, 3.0f);
        glutSolidSphere(0.5, 20, 20);
        glPopMatrix();
    }
    glPopMatrix();
}

void drawDripTrayPetal() {
    glPushMatrix();
    GLfloat mat_amb[] = { 0.8f, 0.8f, 0.75f, 1.0f };
    GLfloat mat_diff[] = { 0.92f, 0.92f, 0.88f, 1.0f };
    GLfloat mat_spec[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    setMaterial(mat_amb, mat_diff, mat_spec, 30.0f);

    glPushMatrix();
    glScalef(1.2f, 0.1f, 3.0f);
    glScalef(1.0f, 0.6f, 1.0f);
    glutSolidSphere(0.5, 30, 30);
    glPopMatrix();

    glTranslatef(0.0f, 0.1f, 0.0f);
    GLfloat metal_amb[] = { 0.3f, 0.25f, 0.2f, 1.0f };
    GLfloat metal_diff[] = { 0.7f, 0.6f, 0.4f, 1.0f };
    GLfloat metal_spec[] = { 0.9f, 0.8f, 0.6f, 1.0f };
    setMaterial(metal_amb, metal_diff, metal_spec, 60.0f);

    glPushMatrix();
    glScalef(1.0f, 0.05f, 2.6f);
    glutSolidSphere(0.5, 20, 20);
    glPopMatrix();

    if (!isRenderingShadow) {
        glDisable(GL_LIGHTING);
        glColor3f(0.8f, 0.7f, 0.5f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for(float z = -1.0f; z <= 1.0f; z+=0.2f) {
            glVertex3f(-0.4f, 0.06f, z);
            glVertex3f( 0.4f, 0.06f, z);
        }
        glEnd();
        glEnable(GL_LIGHTING);
    }
    glPopMatrix();
}

void drawBase() {
    glPushMatrix();
    GLfloat base_amb[] = { 0.3f, 0.25f, 0.2f, 1.0f };
    GLfloat base_diff[] = { 0.7f, 0.65f, 0.5f, 1.0f };
    GLfloat base_spec[] = { 0.8f, 0.8f, 0.6f, 1.0f };
    setMaterial(base_amb, base_diff, base_spec, 50.0f);
    drawSolidFrustum(1.6f, 1.2f, 0.6f);
    glPopMatrix();

    for(int i = 1; i < 8; i++) {
        glPushMatrix();
        glRotatef(i * 45.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(0.0f, 0.3f, 1.3f);
        glRotatef(-5.0f, 1.0f, 0.0f, 0.0f);
        drawLeaf();
        glPopMatrix();
    }

    glPushMatrix();
    glRotatef(0.0f, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, 0.25f, 1.3f);
    glRotatef(0.0f, 1.0f, 0.0f, 0.0f);
    drawDripTrayPetal();
    glPopMatrix();
}

void drawCoils() {
    if(isRenderingShadow) return;

    glPushMatrix();
    GLfloat copper_amb[] = { 0.3f, 0.1f, 0.05f, 1.0f };
    GLfloat copper_diff[] = { 0.9f, 0.4f, 0.2f, 1.0f };
    GLfloat copper_spec[] = { 1.0f, 0.8f, 0.6f, 1.0f };
    setMaterial(copper_amb, copper_diff, copper_spec, 100.0f);

    glLineWidth(2.5f);

    glBegin(GL_LINE_STRIP);
    for(float t = 0; t < 50.0f; t += 0.1f) {
        float r = 0.3f;
        float x = r * cos(t);
        float z = r * sin(t);
        float y = t * 0.1f;
        glVertex3f(x, y, z);
    }
    glEnd();

    glBegin(GL_LINE_STRIP);
    for(float t = 0; t < 50.0f; t += 0.1f) {
        float r = 0.6f;
        float x = r * cos(t + 3.14);
        float z = r * sin(t + 3.14);
        float y = t * 0.1f;
        glVertex3f(x, y, z);
    }
    glEnd();
    glPopMatrix();
}

void drawBezierBlade() {
    glPushMatrix();
    if (!isRenderingShadow) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDisable(GL_CULL_FACE);

    GLfloat fan_amb[] = { 0.5f, 0.45f, 0.3f, 0.9f };
    GLfloat fan_diff[] = { 0.85f, 0.8f, 0.65f, 0.9f };
    GLfloat fan_spec[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    setMaterial(fan_amb, fan_diff, fan_spec, 20.0f);

    glMapGrid2f(12, 0.0, 1.0, 12, 0.0, 1.0);
    glEvalMesh2(GL_FILL, 0, 12, 0, 12);

    if (!isRenderingShadow) {
        glColor4f(0.25f, 0.15f, 0.05f, 0.7f);
        glLineWidth(1.5f);

        glPushMatrix();
        glTranslatef(0.0f, 0.005f, 0.0f);
        glEvalMesh2(GL_LINE, 0, 12, 0, 12);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, -0.005f, 0.0f);
        glEvalMesh2(GL_LINE, 0, 12, 0, 12);
        glPopMatrix();

        glDisable(GL_BLEND);
    }
    glPopMatrix();
}

void drawFanSystem(float angle) {
    glPushMatrix();
    glTranslatef(0.0f, 5.2f, 0.0f);
    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    for(int i=0; i<4; i++) {
        glPushMatrix();
        glRotatef(i * 90, 0.0f, 1.0f, 0.0f);
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.5f, 0.5f, 0.5f);
        drawBezierBlade();
        glPopMatrix();
    }

    GLfloat bearing_amb[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat bearing_diff[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat bearing_spec[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    setMaterial(bearing_amb, bearing_diff, bearing_spec, 50.0f);
    glutSolidSphere(0.3, 20, 20);
    glPopMatrix();
}


void drawSpout() {
    glPushMatrix();
    glTranslatef(0.0f, 0.6f, 1.0f);

    GLfloat steel_amb[] = { 0.3f, 0.3f, 0.35f, 1.0f };
    GLfloat steel_diff[] = { 0.8f, 0.8f, 0.9f, 1.0f };
    GLfloat steel_spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    setMaterial(steel_amb, steel_diff, steel_spec, 120.0f);

    glRotatef(20.0f, 1.0f, 0.0f, 0.0f);

    GLUquadric *q = gluNewQuadric();
    gluCylinder(q, 0.12, 0.1, 0.6, 20, 20);
    glutSolidTorus(0.05, 0.14, 10, 20);

    glPushMatrix();
    glTranslatef(0.0f, 0.15f, 0.3f);

    glPushMatrix();
    glScalef(1.0f, 0.5f, 1.0f);
    glutSolidSphere(0.08, 10, 10);
    glPopMatrix();

    glRotatef(valveAngle, 0.0f, 1.0f, 0.0f);

    GLfloat handle_amb[] = { 0.3f, 0.05f, 0.05f, 1.0f };
    GLfloat handle_diff[] = { 1.0f, 0.2f, 0.2f, 1.0f };
    GLfloat handle_spec[] = { 0.5f, 0.1f, 0.1f, 1.0f };
    setMaterial(handle_amb, handle_diff, handle_spec, 40.0f);

    glPushMatrix();
    glTranslatef(0.0f, 0.05f, 0.0f);
    glScalef(2.5f, 0.5f, 0.5f);
    glutSolidCube(0.1);
    glPopMatrix();

    glPopMatrix();

    if (isFaucetOpen && !isRenderingShadow) {
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.6f + dropY);
        glEnable(GL_BLEND);
        GLfloat water_amb[] = { 0.0f, 0.1f, 0.3f, 0.7f };
        GLfloat water_diff[] = { 0.0f, 0.4f, 0.8f, 0.7f };
        GLfloat water_spec[] = { 0.5f, 0.7f, 1.0f, 0.7f };
        setMaterial(water_amb, water_diff, water_spec, 80.0f);
        glScalef(0.6f, 0.6f, 1.2f);
        glutSolidSphere(0.08, 10, 10);
        glDisable(GL_BLEND);
        glPopMatrix();
    }

    glPopMatrix();
    gluDeleteQuadric(q);
}

void drawGlassStalk() {
    glPushMatrix();

    // Only enable blending (transparency) if NOT rendering shadows
    if (!isRenderingShadow) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (waterLevel > 0.01f) {
        glPushMatrix();
        // Water material
        GLfloat water_amb[] = { 0.0f, 0.1f, 0.3f, 0.6f };
        GLfloat water_diff[] = { 0.0f, 0.3f, 0.9f, 0.6f };
        GLfloat no_spec[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        setMaterial(water_amb, water_diff, no_spec, 10.0f);

        glRotatef(-90, 1, 0, 0);
        // Water level geometry calculation
        float glassBaseR = 0.8f; float glassTopR = 0.5f; float glassHeight = 5.0f; float thickness = 0.05f;
        float waterBaseR = glassBaseR - thickness;
        float currentGlassR = glassBaseR + (waterLevel / glassHeight) * (glassTopR - glassBaseR);
        float waterCurrentTopR = currentGlassR - thickness;
        GLUquadric *waterQuad = gluNewQuadric();
        gluCylinder(waterQuad, waterBaseR, waterCurrentTopR, waterLevel, 30, 1);
        glPushMatrix(); glTranslatef(0.0f, 0.0f, waterLevel); gluDisk(waterQuad, 0.0, waterCurrentTopR, 30, 1); glPopMatrix();
        glPopMatrix();
        gluDeleteQuadric(waterQuad);
    }

    // Enable depth writes for shadows to ensure correct projection
    if (!isRenderingShadow) {
        glDepthMask(GL_FALSE);
    }

    GLfloat glass_amb[] = { 0.3f, 0.4f, 0.5f, 0.15f };
    GLfloat glass_diffuse[] = { 0.7f, 0.9f, 1.0f, 0.15f };
    GLfloat glass_spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    setMaterial(glass_amb, glass_diffuse, glass_spec, 100.0f);

    glRotatef(-90, 1, 0, 0);
    GLUquadric *quad = gluNewQuadric();
    gluCylinder(quad, 0.8, 0.5, 5.0, 40, 40);

    // Restore state
    if (!isRenderingShadow) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glPopMatrix();
    gluDeleteQuadric(quad);
}

// Shadow Casters: Geometry used specifically for casting shadows
void drawShadowCasters() {
    drawDisplayBase();
    drawBase();
    // Draw actual glass geometry for correct shadow shape
    drawGlassStalk();
    drawSpout();
    drawFanSystem(fanRotation);
}



void updateParticles() {
    float coilOuterRadius = 0.6f;
    float fanHeight = 5.2f;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle &p = particles[i];

        if (p.state == STATE_AIR) {
            p.y -= p.speed;
            float distanceToFan = p.y - fanHeight;
            if (distanceToFan > 0) {
                float attractionFactor = 1.0f / (1.0f + distanceToFan * 0.5f);
                p.angle -= 0.03f * (1.0f + attractionFactor * 2.0f);
                p.radius -= 0.01f * (1.0f + attractionFactor);
                p.x = p.radius * cos(p.angle);
                p.z = p.radius * sin(p.angle);
            }
            if (p.y < 5.0f && p.radius <= coilOuterRadius + 0.1f) {
                if (randomFloat(0.0f, 1.0f) > 0.1f) {
                    p.state = STATE_CONDENSING;
                    p.t_coil = p.y * 10.0f;
                    p.radius = coilOuterRadius;
                }
            }
            if (p.y < 0.0f || p.radius < 0.2f) {
                p.y = randomFloat(5.5f, 7.5f);
                p.radius = randomFloat(1.0f, 2.5f);
                p.angle = randomFloat(0.0f, 6.28f);
                p.state = STATE_AIR;
                p.alpha = randomFloat(0.3f, 0.6f);
            }
        }
        else if (p.state == STATE_CONDENSING) {
            p.t_coil -= 0.15f;
            p.x = coilOuterRadius * cos(p.t_coil + 3.14f);
            p.z = coilOuterRadius * sin(p.t_coil + 3.14f);
            p.y = p.t_coil * 0.1f;
            if (p.y < 0.5f) {
                if (isFaucetOpen) {
                    p.state = STATE_FALLING;
                } else {
                    if (waterLevel < maxWaterLevel) waterLevel += 0.02f;
                    p.state = STATE_AIR;
                    p.y = randomFloat(5.5f, 7.5f);
                    p.radius = randomFloat(1.0f, 3.5f);
                    p.angle = randomFloat(0.0f, 6.28f);
                    p.alpha = randomFloat(0.3f, 0.6f);
                }
            }
        }
        else if (p.state == STATE_FALLING) {
            p.y -= 0.15f;
            if (p.y < 0.1f) {
                p.state = STATE_AIR;
                p.y = randomFloat(5.5f, 7.5f);
                p.radius = randomFloat(1.0f, 3.5f);
                p.angle = randomFloat(0.0f, 6.28f);
                p.alpha = randomFloat(0.3f, 0.6f);
            }
        }
    }
}

void drawParticles() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle &p = particles[i];

        glPushMatrix();
        glTranslatef(p.x, p.y, p.z);

        if (p.state == STATE_AIR) {
            float adjustedAlpha = p.alpha * airParticleDensity;
            GLfloat air_col[] = { 0.85f, 0.95f, 1.0f, adjustedAlpha };
            glMaterialfv(GL_FRONT, GL_DIFFUSE, air_col);
            glMaterialfv(GL_FRONT, GL_EMISSION, air_col);
            glutSolidSphere(0.025, 6, 6);
            GLfloat no_emission[] = {0,0,0,1};
            glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
        }
        else {
            GLfloat water_amb[] = { 0.05f, 0.15f, 0.3f, 0.8f };
            GLfloat water_diff[] = { 0.2f, 0.6f, 1.0f, 0.8f };
            GLfloat water_spec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            setMaterial(water_amb, water_diff, water_spec, 100.0f);
            float size = (p.state == STATE_FALLING) ? 0.05f : 0.04f;
            glutSolidSphere(size, 8, 8);
        }

        glPopMatrix();
    }

    glDisable(GL_BLEND);
}

void drawOasisSpire() {
    drawDisplayBase();
    drawBase();
    drawCoils();
    drawGlassStalk();
    drawSpout();
    drawFanSystem(fanRotation);
    drawSmartScreen();
}

void renderBitmapString(float x, float y, void *font, const char *string) {
    const char *c;
    glRasterPos2f(x, y);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

int getBitmapTextWidth(void* font, const char* string) {
    int len = 0;
    for (const char* c = string; *c != '\0'; c++) {
        len += glutBitmapWidth(font, *c);
    }
    return len;
}

void drawControlsOverlay() {
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    void* font = GLUT_BITMAP_9_BY_15;
    int lineHeight = 25;
    int startX = 30;
    int startY = windowHeight - 100;

    renderBitmapString(startX, startY, font, "====== CONTROLS ======");
    renderBitmapString(startX, startY - lineHeight * 2, font,  "F : Toggle Faucet");
    renderBitmapString(startX, startY - lineHeight * 3, font,  "A : Auto-Rotation");
    renderBitmapString(startX, startY - lineHeight * 4, font,  "R : Reset System");
    renderBitmapString(startX, startY - lineHeight * 5, font,  "+/- : Particle Density");
    renderBitmapString(startX, startY - lineHeight * 6, font,  "1-5 : Camera Views");
    renderBitmapString(startX, startY - lineHeight * 7, font,  "W/S : Zoom In/Out");
    renderBitmapString(startX, startY - lineHeight * 8, font,  "Drag: Rotate View");
    renderBitmapString(startX, startY - lineHeight * 9, font, "ESC : Exit");

    char statusBuf[64];
    sprintf(statusBuf, "Status: %s", isFaucetOpen ? "Dispensing" : "Collecting");
    renderBitmapString(startX, 50, font, statusBuf);

    float productCenterX = uiWidth + (windowWidth - uiWidth) / 2.0f;
    const char* title1 = "OASIS SPIRE";
    void* titleFont = GLUT_BITMAP_TIMES_ROMAN_24;
    int t1Width = getBitmapTextWidth(titleFont, title1);

    glColor3f(0.0f, 0.8f, 1.0f);
    renderBitmapString(productCenterX - t1Width / 2, windowHeight - 50, titleFont, title1);

    const char* title2 = "Zero-Maintenance Air-to-Water System";
    void* subFont = GLUT_BITMAP_HELVETICA_18;
    int t2Width = getBitmapTextWidth(subFont, title2);

    glColor3f(0.7f, 0.8f, 0.9f);
    renderBitmapString(productCenterX - t2Width / 2, windowHeight - 80, subFont, title2);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(uiWidth, 0, windowWidth - uiWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)(windowWidth - uiWidth) / (float)windowHeight;
    gluPerspective(45.0, aspect, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (autoRotate) {
        angleY = autoRotateAngle;
    }

    gluLookAt(0.0f, 3.0f, viewDistance,
              0.0f, 2.5f, 0.0f,
              0.0f, 1.0f, 0.0f);

    glRotatef(angleX, 1.0f, 0.0f, 0.0f);

    // 1. Draw Ground
    drawGround();

    // 2. Draw Projected Shadows
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable Stencil Test to prevent shadow overlapping artifacts (double darkening)
    glEnable(GL_STENCIL_TEST);
    glClear(GL_STENCIL_BUFFER_BIT); // Clear stencil buffer
    // Stencil rule: Only draw if stencil value != 1
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    // Stencil op: Replace value with 1 upon success
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // Enable polygon offset to prevent z-fighting with the ground
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    glColor4f(0.0f, 0.0f, 0.0f, 0.3f); // 30% translucent black shadow
    isRenderingShadow = true;

    glPushMatrix();
    myShadowMatrix(g_groundPlane, g_lightPos);
    glRotatef(angleY, 0.0f, 1.0f, 0.0f);
    drawShadowCasters();
    glPopMatrix();

    isRenderingShadow = false;

    // Restore state
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_STENCIL_TEST); // Disable stencil test to allow normal drawing
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);


    // 3. Draw Actual Objects
    glPushMatrix();
    glRotatef(angleY, 0.0f, 1.0f, 0.0f);
    drawOasisSpire();
    drawParticles();
    glPopMatrix();

    // 4. Draw UI Overlay
    glViewport(0, 0, windowWidth, windowHeight);
    drawControlsOverlay();

    glutSwapBuffers();
}

void timer(int value) {
    fanRotation += 5.0f;
    if (fanRotation > 360) fanRotation -= 360;

    float targetAngle = isFaucetOpen ? 0.0f : 90.0f;
    valveAngle += (targetAngle - valveAngle) * 0.1f;

    if (isFaucetOpen) {
        if (waterLevel > 0.0f) {
            waterLevel -= 0.03f;
            if (waterLevel < 0.0f) waterLevel = 0.0f;
        }
    } else {
        if (waterLevel < maxWaterLevel) {
            waterLevel += 0.015f;
        }
    }

    if (autoRotate) {
        autoRotateAngle += 0.5f;
        if (autoRotateAngle > 360) autoRotateAngle -= 360;
    }

    dropY += 0.08f;
    if (dropY > 1.5f) dropY = 0.0f;

    screenAnimTime += 0.016f;
    blinkTimer += 0.1f;
    if (screenAnimTime > 1.0f) {
        currentHumidity = 82.5f + sin(fanRotation * 0.05f) * 2.0f;
        currentPurity = 99.8f + (rand() % 2) * 0.1f;
        screenAnimTime = 0.0f;
    }

    updateParticles();

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void mouseMotion(int x, int y) {
    if (autoRotate) return;
    int dx = x - lastX;
    int dy = y - lastY;
    angleY += dx * 0.5f;
    angleX += dy * 0.5f;
    lastX = x;
    lastY = y;
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        lastX = x;
        lastY = y;
    }

    if (state == GLUT_UP) return;

    if (button == 3) {
        viewDistance -= 1.0f;
    }
    else if (button == 4) {
        viewDistance += 1.0f;
    }

    if (viewDistance < 2.0f) viewDistance = 2.0f;
    if (viewDistance > 40.0f) viewDistance = 40.0f;

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':
        case 'W':
            viewDistance -= 0.5f;
            break;
        case 's':
        case 'S':
            viewDistance += 0.5f;
            break;
        case 'f':
        case 'F':
            isFaucetOpen = !isFaucetOpen;
            break;
        case 'a':
        case 'A':
            autoRotate = !autoRotate;
            break;
        case 'r':
        case 'R':
            waterLevel = 0.0f;
            isFaucetOpen = true;
            valveAngle = 0.0f;
            initParticles();
            break;
        case '+':
        case '=':
            airParticleDensity += 0.1f;
            if (airParticleDensity > 1.0f) airParticleDensity = 1.0f;
            break;
        case '-':
        case '_':
            airParticleDensity -= 0.1f;
            if (airParticleDensity < 0.0f) airParticleDensity = 0.0f;
            break;
        case '1':
            angleX = 15.0f; angleY = -30.0f; viewDistance = 13.0f;
            autoRotate = false;
            break;
        case '2':
            angleX = 0.0f; angleY = 0.0f; viewDistance = 10.0f;
            autoRotate = false;
            break;
        case '3':
            angleX = 0.0f; angleY = 90.0f; viewDistance = 10.0f;
            autoRotate = false;
            break;
        case '4':
            angleX = 90.0f; angleY = 0.0f; viewDistance = 15.0f;
            autoRotate = false;
            break;
        case '5':
            angleX = 30.0f; angleY = 45.0f; viewDistance = 18.0f;
            autoRotate = false;
            break;
        case 27: // ESC
            exit(0);
            break;
    }

    if (viewDistance < 2.0f) viewDistance = 2.0f;
    if (viewDistance > 40.0f) viewDistance = 40.0f;

    glutPostRedisplay();
}

void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w/h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);

     // Request a window with Double Buffer, RGB, Depth, and Stencil buffer
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);

    glutInitWindowSize(1000, 700);
    glutCreateWindow("CST2309168");
    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
}
