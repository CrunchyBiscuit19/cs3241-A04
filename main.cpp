// CS3241Lab4.cpp : Defines the entry point for the console application.
// #include <cmath>
#include <fstream>
#include <iostream>

#include "math.h"

#ifdef _WIN32
#include <Windows.h>

#include "GL/glut.h"
#define M_PI 3.141592654
#elif __APPLE__
#include <GLUT/GLUT.h>
#include <OpenGL/gl.h>
#endif

#define MAXPTNO 1000
#define NLINESEGMENT 32
#define NOBJECTONCURVE 8

using namespace std;

// Global variables that you can use
struct Point {
    int x, y;
};

// Storage of control points
int numPoints = 0;
Point ptList[MAXPTNO];

// Display options
bool displayControlPoints = true;
bool displayControlLines = true;
bool displayTangentVectors = false;
bool displayObjects = false;
bool C1Continuity = false;

void drawRightArrow() {
    glColor3f(0, 1, 0);
    glBegin(GL_LINE_STRIP);
    glVertex2f(0, 0);
    glVertex2f(100, 0);
    glVertex2f(95, 5);
    glVertex2f(100, 0);
    glVertex2f(95, -5);
    glEnd();
}

bool c1Moved(int i) { return C1Continuity && i > 3 && i % 3 == 1; }

Point cp(int i) {
    if (c1Moved(i)) return {2 * ptList[i - 1].x - ptList[i - 2].x, 2 * ptList[i - 1].y - ptList[i - 2].y};
    return ptList[i];
}

void bezier(int i, float t, float& x, float& y, float& dx, float& dy) {
    Point p0 = cp(i), p1 = cp(i + 1), p2 = cp(i + 2), p3 = cp(i + 3);
    float s = 1 - t;
    x = s * s * s * p0.x + 3 * t * s * s * p1.x + 3 * t * t * s * p2.x + t * t * t * p3.x;
    y = s * s * s * p0.y + 3 * t * s * s * p1.y + 3 * t * t * s * p2.y + t * t * t * p3.y;
    dx = 3 * s * s * (p1.x - p0.x) + 6 * t * s * (p2.x - p1.x) + 3 * t * t * (p3.x - p2.x);
    dy = 3 * s * s * (p1.y - p0.y) + 6 * t * s * (p2.y - p1.y) + 3 * t * t * (p3.y - p2.y);
}

void drawObject() {
    glColor3f(1, 0.5f, 0);
    glBegin(GL_POLYGON);  // body
    for (int i = 0; i < 20; i++) glVertex2d(15 * cos(i * M_PI / 10), 7 * sin(i * M_PI / 10));
    glEnd();
    glBegin(GL_TRIANGLES);  // tail
    glVertex2f(-12, 0);
    glVertex2f(-22, 8);
    glVertex2f(-22, -8);
    glEnd();
    glColor3f(0, 0, 0);
    glPointSize(3);
    glBegin(GL_POINTS);  // eye
    glVertex2f(8, -2);
    glEnd();
    glPointSize(1);
}

// Draw NOBJECTONCURVE copies per curve in the tangent/normal frame
void drawAlongCurves(void (*draw)()) {
    for (int i = 0; i + 3 < numPoints; i += 3)
        for (int j = 0; j < NOBJECTONCURVE; j++) {
            float x, y, dx, dy;
            bezier(i, j / (NOBJECTONCURVE - 1.0f), x, y, dx, dy);
            glPushMatrix();
            glTranslatef(x, y, 0);
            glRotatef((float)(atan2(dy, dx) * 180 / M_PI), 0, 0, 1);
            draw();
            glPopMatrix();
        }
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();

    if (displayControlLines) {
        glColor3f(0, 1, 0);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < numPoints; i++) glVertex2d(cp(i).x, cp(i).y);
        glEnd();
    }

    glColor3f(0, 0, 1);
    for (int i = 0; i + 3 < numPoints; i += 3) {
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j <= NLINESEGMENT; j++) {
            float x, y, dx, dy;
            bezier(i, (float)j / NLINESEGMENT, x, y, dx, dy);
            glVertex2f(x, y);
        }
        glEnd();
    }

    if (displayObjects) drawAlongCurves(drawObject);
    if (displayTangentVectors) drawAlongCurves(drawRightArrow);

    if (displayControlPoints) {
        glPointSize(5);
        glBegin(GL_POINTS);
        for (int i = 0; i < numPoints; i++) {
            if (c1Moved(i)) {
                glColor3f(0.7f, 0.7f, 0.7f);
                glVertex2d(ptList[i].x, ptList[i].y);
                glColor3f(1, 0, 0);
            } else
                glColor3f(0, 0, 0);
            glVertex2d(cp(i).x, cp(i).y);
        }
        glEnd();
        glPointSize(1);
    }

    glPopMatrix();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, h, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void init(void) { glClearColor(1.0, 1.0, 1.0, 1.0); }

void readFile() {
    std::ifstream file;
    file.open("savefile.txt");
    file >> numPoints;

    if (numPoints > MAXPTNO) {
        cout << "Error: File contains more than the maximum number of points." << endl;
        numPoints = MAXPTNO;
    }

    for (int i = 0; i < numPoints; i++) {
        file >> ptList[i].x;
        file >> ptList[i].y;
    }
    file.close();  
}

void writeFile() {
    std::ofstream file;
    file.open("savefile.txt");
    file << numPoints << endl;

    for (int i = 0; i < numPoints; i++) {
        file << ptList[i].x << " ";
        file << ptList[i].y << endl;
    }
    file.close();  
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'r':
        case 'R':
            readFile();
            break;

        case 'w':
        case 'W':
            writeFile();
            break;

        case 'T':
        case 't':
            displayTangentVectors = !displayTangentVectors;
            break;

        case 'o':
        case 'O':
            displayObjects = !displayObjects;
            break;

        case 'p':
        case 'P':
            displayControlPoints = !displayControlPoints;
            break;

        case 'L':
        case 'l':
            displayControlLines = !displayControlLines;
            break;

        case 'C':
        case 'c':
            C1Continuity = !C1Continuity;
            break;

        case 'e':
        case 'E':
            numPoints = 0;
            break;

        case 'Q':
        case 'q':
            exit(0);
            break;

        default:
            break;
    }

    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    /*button: GLUT_LEFT_BUTTON, GLUT_MIDDLE_BUTTON, or GLUT_RIGHT_BUTTON */
    /*state: GLUT_UP or GLUT_DOWN */
    enum { MOUSE_LEFT_BUTTON = 0, MOUSE_MIDDLE_BUTTON = 1, MOUSE_RIGHT_BUTTON = 2, MOUSE_SCROLL_UP = 3, MOUSE_SCROLL_DOWN = 4 };
    if ((button == MOUSE_LEFT_BUTTON) && (state == GLUT_UP)) {
        if (numPoints == MAXPTNO) {
            cout << "Error: Exceeded the maximum number of points." << endl;
            return;
        }
        ptList[numPoints].x = x;
        ptList[numPoints].y = y;
        numPoints++;
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    cout << "CS3241 Lab 4" << endl << endl;
    cout << "Left mouse click: Add a control point" << endl;
    cout << "Q: Quit" << endl;
    cout << "P: Toggle displaying control points" << endl;
    cout << "L: Toggle displaying control lines" << endl;
    cout << "E: Erase all points (Clear)" << endl;
    cout << "C: Toggle C1 continuity" << endl;
    cout << "T: Toggle displaying tangent vectors" << endl;
    cout << "O: Toggle displaying objects" << endl;
    cout << "R: Read in control points from \"savefile.txt\"" << endl;
    cout << "W: Write control points to \"savefile.txt\"" << endl;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutInitWindowPosition(50, 50);
    glutCreateWindow("CS3241 Assignment 4");
    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutMainLoop();

    return 0;
}
