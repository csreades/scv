
// Visualization for scv planner using Dear ImGui, OpenGL2 implementation.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <GLFW/glfw3.h>

#include <chrono>
#include "implot.h"
#include "planner.h"
#include "camera.h"

using namespace std;
using namespace scv;

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#include <fstream>
#include <sstream>
#include <filesystem>

GLFWwindow* window;

Camera camera;      // handles a FPS game style input (WASD, left shift, left ctrl)
planner plan;       // the S-curve planner we're testing

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
void my_gluPerspective(float fovY, float aspect, float zNear, float zFar)
{
    float f = (scv_float)tan( M_PI / 2 - fovY / 2);
    float m[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (zFar + zNear) / (zNear - zFar), -1,
        0, 0, (2 * zFar * zNear) / (zNear - zFar), 0
    };
    glMultMatrixf(m);
}

// These are to cycle between red/green/blue when drawing segments to differentiate them
vec3 colors[] = {
    vec3(0.9f, 0.3f, 0.3f),
    vec3(0.3f, 0.9f, 0.3f),
    vec3(0.3f, 0.3f, 0.9f)
};

// Draw 3 simple lines to show the world origin location
void drawAxes() {

    glLineWidth(3);
    glBegin(GL_LINES);

    glColor3f(1,0,0);
    glVertex3f(0,0,0);
    glVertex3f(1,0,0);

    glColor3f(0,1,0);
    glVertex3f(0,0,0);
    glVertex3f(0,1,0);

    glColor3f(0,0,1);
    glVertex3f(0,0,0);
    glVertex3f(0,0,1);

    glEnd();
}

// Draw a simple bounding box to show the position constraints of the planner
void drawPlannerBoundingBox() {
    glLineWidth(1);
    glColor3f( 0.f, 0.66f, 0.f);
    glBegin(GL_LINE_LOOP);
    glVertex3d( plan.posLimitLower.x, plan.posLimitLower.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitLower.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitUpper.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitUpper.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitUpper.y, plan.posLimitUpper.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitUpper.y, plan.posLimitUpper.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitLower.y, plan.posLimitUpper.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitLower.y, plan.posLimitUpper.z );
    glEnd();
    glBegin(GL_LINES);
    glVertex3d( plan.posLimitUpper.x, plan.posLimitLower.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitLower.y, plan.posLimitUpper.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitUpper.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitUpper.x, plan.posLimitUpper.y, plan.posLimitUpper.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitLower.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitUpper.y, plan.posLimitLower.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitLower.y, plan.posLimitUpper.z );
    glVertex3d( plan.posLimitLower.x, plan.posLimitUpper.y, plan.posLimitUpper.z );
    glEnd();
}

// These arrays will be used to draw the plots
#define MAXPLOTPOINTS 1024
float plotTime[MAXPLOTPOINTS]; // x axis of the plot, all others are y-axis values
float plotPosX[MAXPLOTPOINTS], plotPosY[MAXPLOTPOINTS], plotPosZ[MAXPLOTPOINTS];
float plotVelX[MAXPLOTPOINTS], plotVelY[MAXPLOTPOINTS], plotVelZ[MAXPLOTPOINTS];
float plotAccX[MAXPLOTPOINTS], plotAccY[MAXPLOTPOINTS], plotAccZ[MAXPLOTPOINTS];
float plotJerkX[MAXPLOTPOINTS], plotJerkY[MAXPLOTPOINTS], plotJerkZ[MAXPLOTPOINTS];
float plotVelMag[MAXPLOTPOINTS], plotAccMag[MAXPLOTPOINTS], plotJerkMag[MAXPLOTPOINTS];
int numPlotPoints = 0; // how many plot points are actually filled

// These are used to get a moving average of the time taken to calculate the full trajectory
#define NUMCALCTIMES 64
float calcTimes[NUMCALCTIMES];
float calcTimeTotal = 0;
int calcTimeInd = 0;
float calcTime = 0; // final result to show in GUI

float animAdvance = 0;  // used to animate a white dot moving along the path
vec3 animLoc;
bool showBoundingBox = true;
bool showControlPoints = true;

bool haveViolation = false;

bool violation(vec3& p, vec3& j, vec3& dp, vec3& dv, vec3& da) {
    double margin = 1.0001;
    if ( p.x < plan.posLimitLower.x * margin ) return true;
    if ( p.y < plan.posLimitLower.y * margin ) return true;
    if ( p.z < plan.posLimitLower.z * margin ) return true;
    if ( p.x > plan.posLimitUpper.x * margin ) return true;
    if ( p.y > plan.posLimitUpper.y * margin ) return true;
    if ( p.z > plan.posLimitUpper.z * margin ) return true;
    if ( fabs(dp.x) > plan.velLimit.x * margin ) return true;
    if ( fabs(dp.y) > plan.velLimit.y * margin ) return true;
    if ( fabs(dp.z) > plan.velLimit.z * margin ) return true;
    if ( fabs(dv.x) > plan.accLimit.x * margin ) return true;
    if ( fabs(dv.y) > plan.accLimit.y * margin ) return true;
    if ( fabs(dv.z) > plan.accLimit.z * margin ) return true;
    if ( fabs(da.x) > plan.jerkLimit.x * margin ) return true;
    if ( fabs(da.y) > plan.jerkLimit.y * margin ) return true;
    if ( fabs(da.z) > plan.jerkLimit.z * margin ) return true;
    if ( fabs(j.x) > plan.jerkLimit.x * margin ) return true;
    if ( fabs(j.y) > plan.jerkLimit.y * margin ) return true;
    if ( fabs(j.z) > plan.jerkLimit.z * margin ) return true;
    return false;
}

// This function will be called during ImGui's rendering, while drawing the background.
// Draw all our stuff here so the GUI windows will then be over the top of it. Need to
// re-enable scissor test after we're done.
void backgroundRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {

    IM_UNUSED(parent_list);
    IM_UNUSED(cmd);

    haveViolation = false;

    // calculate the trajectory every frame, and measure the time taken
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    plan.calculateMoves();
    plan.calculateSchedules();
    std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();

    // update the moving average
    long int timeTaken = (long int)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    calcTimeTotal -= calcTimes[calcTimeInd];
    calcTimeTotal += timeTaken;
    calcTimes[calcTimeInd] = (scv_float)timeTaken;
    calcTimeInd = (calcTimeInd + 1) % NUMCALCTIMES;
    calcTime = 0;
    for (int i = 0; i < NUMCALCTIMES; i++) {
        calcTime += calcTimes[i];
    }
    calcTime /= NUMCALCTIMES;

    // apply view transforms
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    float aspectRatio = w / (float)h;

    glDisable(GL_SCISSOR_TEST);

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    my_gluPerspective(45, aspectRatio, 1, 1000);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    camera.gluLookAt();

    drawAxes();

    if ( showBoundingBox ) {
        drawPlannerBoundingBox();
    }


    vec3 vp = vec3_zero;

    numPlotPoints = 0;

    glPointSize(4);
    glBegin(GL_POINTS);

    vec3 p, v, a, j; // position, velocity, acceleration, jerk
    vec3 lp, lv, la;
    
    p = vec3_zero;
    v = vec3_zero;
    a = vec3_zero;
    
	lp = p;
	lv = v;
	la = a;
    
    scv_float t = 0;
    int count = 0;
    int segmentIndex;
    while ( true ) {

        if ( t > 0.7 ) {
            int adf = 3;
            adf += 2;
        }

        bool stillOnPath = false;
        if ( plan.blendMethod == CBM_INTERPOLATED_MOVES )
            stillOnPath = plan.getTrajectoryState_interpolatedMoves(t, &segmentIndex, &p, &v, &a, &j);
        else
            stillOnPath = plan.getTrajectoryState_constantJerkSegments(t, &segmentIndex, &p, &v, &a, &j);

        vec3 dp = p - lp;
        vec3 dv = v - lv;
        vec3 da = a - la;

        if ( count > 0 && violation(p, j, dp, dv, da) ) {
            haveViolation = true;
            vp = p;
        }

        vec3 c = colors[segmentIndex % 3];
        glColor3f(c.x,c.y,c.z);

        glVertex3f( p.x, p.y, p.z );

        if ( count < MAXPLOTPOINTS ) {
            plotTime[count] = t;
            plotPosX[count] = p.x;
            plotPosY[count] = p.y;
            plotPosZ[count] = p.z;
            plotVelX[count] = v.x;
            plotVelY[count] = v.y;
            plotVelZ[count] = v.z;
            plotAccX[count] = a.x;
            plotAccY[count] = a.y;
            plotAccZ[count] = a.z;
            plotJerkX[count] = j.x;
            plotJerkY[count] = j.y;
            plotJerkZ[count] = j.z;
            plotVelMag[count] = v.Length();
            plotAccMag[count] = a.Length();
            plotJerkMag[count] = j.Length();
        }

        t += 0.01f;
        count++;

        if ( ! stillOnPath )
            break;

        lp = p;
        lv = v;
        la = a;
    }

    /*size_t segmentIndex = 0;
    float timeBase = 0;
    int tCount = 0;
    float timeStepsOverFlow = 0;
    float timePerDivision = 0.01;
    vector<segment>& segments = plan.getSegments();
    while (segmentIndex < segments.size()) {

        scv::segment& s = segments[segmentIndex];

        vec3 c = colors[segmentIndex%3];
        glColor3f(c.x,c.y,c.z);

        float localT = timeStepsOverFlow * timePerDivision;

        while ( localT < s.duration ) {

            float globalT = tCount * timePerDivision;

            scv::vec3 p, v, a, j;
            plan.getScurvePoint(s, localT, &p, &v, &a, &j);

            if ( tCount < MAXPLOTPOINTS ) {
                plotTime[tCount] = globalT;
                plotPosX[tCount] = p.x;
                plotPosY[tCount] = p.y;
                plotPosZ[tCount] = p.z;
                plotVelX[tCount] = v.x;
                plotVelY[tCount] = v.y;
                plotVelZ[tCount] = v.z;
                plotAccX[tCount] = a.x;
                plotAccY[tCount] = a.y;
                plotAccZ[tCount] = a.z;

                plotVelMag[tCount] = v.Length();
                plotAccMag[tCount] = a.Length();
                plotJerkMag[tCount] = j.Length();
            }

            glVertex3f( p.x, p.y, p.z );

            localT += timePerDivision;
            tCount++;
        }

        timeStepsOverFlow = (localT - s.duration) / timePerDivision;
        segmentIndex++;
        timeBase += 1;
    }*/

    glEnd();

    numPlotPoints = scv::min(count-1, MAXPLOTPOINTS);

    if ( showControlPoints ) {
        glPointSize(8);
        glColor3f(1,0,1);
        glBegin(GL_POINTS);
        for (size_t i = 0; i < plan.moves.size(); i++) {
            scv::move& m = plan.moves[i];
            if ( i == 0 )
                glVertex3d( m.src.x, m.src.y, m.src.z );
            glVertex3d( m.dst.x, m.dst.y, m.dst.z );
        }
        glEnd();
    }

    bool animRunning = plan.advanceTraverse( animAdvance, &animLoc );

    glPointSize(12);
    if ( animRunning )
        glColor3f(1,1,1);
    else
        glColor3f(0.5,0.5,0.5);
    glBegin(GL_POINTS);
    glVertex3d( animLoc.x, animLoc.y, animLoc.z );
    glEnd();

    if ( haveViolation ) {
        glPointSize(12);
        glColor3f(1,0,0);
        glBegin(GL_POINTS);
        glVertex3d( vp.x, vp.y, vp.z );
        glEnd();
    }

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glEnable(GL_SCISSOR_TEST);
}

// A convenience function to show vec3 components as individual inputs
void showVec3Editor(const char* label, vec3 *v) {
    char n[128];
    snprintf(n, sizeof(n), "%s X", label);
    ImGui::InputFloat(n, &v->x, 0.1f, 1.0f);
    snprintf(n, sizeof(n), "%s Y", label);
    ImGui::InputFloat(n, &v->y, 0.1f, 1.0f);
    snprintf(n, sizeof(n), "%s Z", label);
    ImGui::InputFloat(n, &v->z, 0.1f, 1.0f);
}

void showPlots() {
    if (ImPlot::BeginPlot("Line Plots", ImVec2(800,600))) {
        ImPlot::SetupAxes("t","");
        ImPlot::PlotLine("Pos X", plotTime, plotPosX, numPlotPoints);
        ImPlot::PlotLine("Pos Y", plotTime, plotPosY, numPlotPoints);
        ImPlot::PlotLine("Pos Z", plotTime, plotPosZ, numPlotPoints);
        ImPlot::PlotLine("Vel X", plotTime, plotVelX, numPlotPoints);
        ImPlot::PlotLine("Vel Y", plotTime, plotVelY, numPlotPoints);
        ImPlot::PlotLine("Vel Z", plotTime, plotVelZ, numPlotPoints);
        ImPlot::PlotLine("Acc X", plotTime, plotAccX, numPlotPoints);
        ImPlot::PlotLine("Acc Y", plotTime, plotAccY, numPlotPoints);
        ImPlot::PlotLine("Acc Z", plotTime, plotAccZ, numPlotPoints);
        ImPlot::PlotLine("Jerk X", plotTime, plotJerkX, numPlotPoints);
        ImPlot::PlotLine("Jerk Y", plotTime, plotJerkY, numPlotPoints);
        ImPlot::PlotLine("Jerk Z", plotTime, plotJerkZ, numPlotPoints);
        ImPlot::PlotLine("Vel mag", plotTime, plotVelMag, numPlotPoints);
        ImPlot::PlotLine("Acc mag", plotTime, plotAccMag, numPlotPoints);
        ImPlot::PlotLine("Jerk mag", plotTime, plotJerkMag, numPlotPoints);
        ImPlot::EndPlot();
    }
}

void randomizePoints() {

    scv::vec3 lastRandPos;

    for (size_t i = 0; i < plan.moves.size(); i++) {
        scv::move& m = plan.moves[i];

        if ( i > 0 )
            m.src = lastRandPos;

        scv::vec3 r;

        if ( i == 0 ) {
            r = scv::vec3(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);
            r = r * plan.posLimitUpper;
            r += plan.posLimitLower;
            m.src = r;
        }

        r = scv::vec3(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);
        r = r * plan.posLimitUpper;
        r += plan.posLimitLower;
        m.dst = r;

        lastRandPos = r;

    }
}








static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void loadTestCase_default() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 7);

    scv::move m;
    m.blendClearance = 0.1f;
    m.vel = 10;
    m.acc = 100;
    m.jerk = 1000;
    m.blendType = CBT_MAX_JERK;
    m.src = vec3( 1, 1, 0);
    m.dst = vec3( 1, 1, 6);    plan.appendMove(m);
    m.dst = vec3( 1, 9, 6);    plan.appendMove(m);
    m.dst = vec3( 1, 9, 0);    plan.appendMove(m);
    m.dst = vec3( 5, 9, 0);    plan.appendMove(m);
    m.dst = vec3( 5, 5, 0);    plan.appendMove(m);
    m.dst = vec3( 5, 1, 6);    plan.appendMove(m);
    m.dst = vec3( 9, 1, 6);    plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);    plan.appendMove(m);
    m.dst = vec3( 9, 9, 0);    plan.appendMove(m);
    m.dst = vec3( 9, 9, 6);    plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

// awkward things can happen when three points are colinear
void loadTestCase_straight() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 10);

    scv::move m;
    m.vel = 6;
    m.acc = 200;
    m.jerk = 800;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3( 1, 1, 0);
    m.dst = vec3( 5, 1, 0);               plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 5);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 9);  m.vel = 6;   plan.appendMove(m);
    m.dst = vec3( 9, 5, 9);  m.vel = 3;   plan.appendMove(m);
    m.dst = vec3( 9, 9, 9);  m.vel = 12;  plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

// even more awkward things can happen when a move is exactly opposite to the previous move
void loadTestCase_retrace() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 10);

    scv::move m;
    m.vel = 12;
    m.acc = 200;
    m.jerk = 800;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3( 1, 1, 0);
    m.dst = vec3( 6, 1, 0);  m.vel = 6;   plan.appendMove(m);
    m.dst = vec3( 3, 1, 0);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);  m.vel = 8;   plan.appendMove(m);
    m.dst = vec3( 9, 1, 5);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);  m.vel = 9;   plan.appendMove(m);
    m.dst = vec3( 9, 1, 9);  m.vel = 6;   plan.appendMove(m);
    m.dst = vec3( 9, 5, 9);  m.vel = 2;   plan.appendMove(m);
    m.dst = vec3( 9, 0.2f, 9); m.vel = 3; plan.appendMove(m);
    m.dst = vec3( 9, 9, 9);  m.vel = 10;  plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

void loadTestCase_malformed() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 7);

    scv::move m;
    m.vel = 0;
    m.acc = 0;
    m.jerk = 0;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3(0, 0, 0);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    m.jerk = 1;
    m.dst = vec3(1, 1, 0);  plan.appendMove(m);
    m.jerk = 0;
    m.dst = vec3(2, 0, 0);  plan.appendMove(m);
    m.jerk = 1;
    m.acc = 1;
    m.dst = vec3(3, 1, 0);  plan.appendMove(m);
    m.vel = 1;
    m.dst = vec3(3, 1, 0);  plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

void saveTrajectoryToFile(const std::string& filename);
void loadTestCase_file(const std::filesystem::path filename)
{

    //auto filename = std::filesystem::absolute(Reffilename); // Convert to absolute path
    plan.clear();
    //plan.setPositionLimits(0, 0, 0, 10, 10, 7);
    std::ifstream file(filename);
    if (!file)
    {
        printf("Failed to open GCode file: %s\n", filename.string().c_str());
        return;
    }
    scv::move m;
    m.vel = 100; //this is the current feed rate
    m.acc = 1000;
    m.jerk = 1000000000;
    m.blendType = CBT_MIN_JERK;

    m.src = vec3(0, 0, 0);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    bool newMove = false;
    bool isMove = false;
    std::string line;
    size_t moveCount = 0;
    vec3 newPos = vec3_zero;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string token;

        while (iss >> token)
        {
            if (token[0] == 'G')
            {
                if (token == "G0")
                    isMove = true;
                if (token == "G1")
                    isMove = true;
            }
            if (token[0] == 'X')
            {
                newPos.x = std::stof(token.substr(1));
                newMove = true;
            }
            else if (token[0] == 'Y')
            {
                newPos.y = std::stof(token.substr(1));
                newMove = true;
            }
            else if (token[0] == 'Z')
            {
                newPos.z = std::stof(token.substr(1));
                newMove = true;
            }
            else if (token[0] == 'E')
            {
                //this is the extruded amount
                //m.extrudeLength = std::stof(token.substr(1));
                newMove = true;
            }
            else if (token[0] == 'F')
            {
                //This is the feed rate
                m.vel = std::stof(token.substr(1));
            }
        }

        if (newMove && isMove)
        {
            m.dst = newPos;
            plan.appendMove(m);
            //printf("Move Aded");
            moveCount++;
        }
        newMove = false;
        isMove = false;
        //m.extrudeLength = 0;

        printf("Move count %d\n", moveCount);
        if (moveCount == 1000)
        {
            break;
        }

    }

    file.close();

    plan.calculateMoves();
    plan.resetTraverse();
    saveTrajectoryToFile("output.txt");
}


bool trajectorySaved = false;
void saveTrajectoryToFile(const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        printf("Error: Could not open file %s for writing!\n", filename.c_str());
        return;
    }

    file << "Time,X,Y,Z\E\n"; // CSV header

    scv_float t = 0;
    int count = 0;
    vec3 v, a, j;
    vec3 p;
    scv_float e = 0;
    int segmentIndex;

    while (true)
    {
        bool stillOnPath = false;
        if (plan.blendMethod == CBM_INTERPOLATED_MOVES)
            stillOnPath = plan.getTrajectoryState_interpolatedMoves(t, &segmentIndex, &p, &v, &a, &j);
        else
            stillOnPath = plan.getTrajectoryState_constantJerkSegments(t, &segmentIndex, &p, &v, &a, &j);

        // Write to file
        file << t << "\t" << p.x << "\t" << p.y << "\t" << p.z << "\t" << e << "\n";

        t += 0.002; // Keep in sync with rendering step

        if (!stillOnPath)
            break;
    }

    file.close();
    printf("Trajectory saved to %s\n", filename.c_str());
}

void loadTestCase_pnp() {
    plan.clear();
    plan.setPositionLimits(0, 0, -40,  400, 480, 0);
    plan.setVelocityLimits(1000, 1000, 1000);
    plan.setAccelerationLimits(50000, 50000, 50000);
    plan.setJerkLimits(100000, 100000, 100000);

    scv::move m;
    m.vel = 300;
    m.acc = 20000;
    m.jerk = 40000;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3(10, 10, -30);

    m.dst = vec3(10, 10, 0);        plan.appendMove(m);
    m.vel = 600;
    m.dst = vec3(100, 100, 0);      plan.appendMove(m);
    m.vel = 300;
    m.dst = vec3(100, 100, -30);    plan.appendMove(m);

    m.blendType = CBT_NONE;
    m.dst = vec3(100, 100, 0);    plan.appendMove(m);
    m.blendType = CBT_MIN_JERK;
    m.vel = 600;
    m.dst = vec3(100, 0, 0);    plan.appendMove(m);
    m.vel = 300;
    m.dst = vec3(100, 0, -30);    plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}



float maxOverlapFraction = 0.28f;

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    window = glfwCreateWindow(1280, 720, "S-Curve visualizer", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    
    // ImGui has a built-in font, but you can load your own here too. Sometimes I forget 
    // to comment this out before committing, so let's check the file exists before trying
    // to use it, to avoid an assertion when the file is not found.
    const char* myFontFile = "/usr/share/fonts/gnu-free/FreeSans.ttf";
    FILE* f;
#ifdef _WIN32
    bool haveFontFile = ( 0 == fopen_s(&f, myFontFile, "r"));
#else
    f = fopen(myFontFile, "r");
    bool haveFontFile = f;
#endif
    if ( haveFontFile ) {
		fclose( f );
		io.Fonts->AddFontFromFileTTF(myFontFile, 16.0f);
	}

    camera.setLocation(-5, -10, 11);
    camera.setDirection( 28, -24 );

    cornerBlendMethod blendMethod = CBM_CONSTANT_JERK_SEGMENTS;
    plan.setCornerBlendMethod(blendMethod);

    plan.setVelocityLimits(20, 20, 20);
    plan.setAccelerationLimits(100, 100, 100);
    plan.setJerkLimits(1000, 1000, 1000);

    loadTestCase_default();

    //plan.printConstraints();
    //plan.printMoves();
    //plan.printSegments();

    //bool show_demo_window = false;
    bool stopOnViolation = false;
    bool doRandomizePoints = false;
    float animSpeedScale = 1;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if ( glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS )
            break;

        if ( io.Framerate != 0 ) // framerate will be zero on first frame
            animAdvance = 1 / io.Framerate * animSpeedScale;

        if ( stopOnViolation && haveViolation )
            doRandomizePoints = false;

        if ( doRandomizePoints ) {
            randomizePoints();
            plan.resetTraverse();
        }

        if ( ImGui::IsMouseDown(1) ) {// hold right mouse button to pan view
            float yaw = camera.yaw + io.MouseDelta.x * 0.05f;
            float pitch = camera.pitch + io.MouseDelta.y * -0.05f;
            camera.setDirection(yaw, pitch);
        }

        float camMoveSpeed = 0.15f;

        float right = 0;
        float forward = 0;
        float up = 0;
        if ( ImGui::IsKeyDown(ImGuiKey_A) ) {
            right = -camMoveSpeed;
        }
        else if ( ImGui::IsKeyDown(ImGuiKey_D) ) {
            right = camMoveSpeed;
        }        

        if ( ImGui::IsKeyDown(ImGuiKey_S) ) {
            forward = -camMoveSpeed;
        }
        else if ( ImGui::IsKeyDown(ImGuiKey_W) ) {
            forward = camMoveSpeed;
        }

        if ( ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ) {
            up = -camMoveSpeed;
        }
        else if ( ImGui::IsKeyDown(ImGuiKey_LeftShift) ) {
            up = camMoveSpeed;
        }
        camera.translate(right, forward, up);

        if ( ImGui::IsKeyDown(ImGuiKey_T) ) {
            plan.resetTraverse();
        }


        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
        bgdl->AddCallback(backgroundRenderCallback, nullptr);

        {
            ImGui::Begin("Settings");

            //ImGui::Checkbox("Demo Window", &show_demo_window);

            ImGui::Text("WASD, left shift, left ctrl to move camera.\n"
                        "Hold right mouse button and drag to rotate camera.\n"
                        "T to restart animation");

            char n[128];

            if (ImGui::CollapsingHeader("Display options"))
            {
                if (ImGui::BeginTable("split", 2))
                {
                    ImGui::TableNextColumn(); ImGui::Checkbox("Show bounding box", &showBoundingBox);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Show control points", &showControlPoints);
                    ImGui::EndTable();
                }


            }

            if (ImGui::CollapsingHeader("Animation"))
            {
                ImGui::SliderFloat("Speed scale", &animSpeedScale, 0, 5);
                showVec3Editor("animLoc", &animLoc);
            }

            if (ImGui::CollapsingHeader("Planner settings"))
            {
                int e = blendMethod;
                ImGui::RadioButton("None", &e, CBM_NONE); ImGui::SameLine();
                ImGui::RadioButton("Constant jerk segments", &e, CBM_CONSTANT_JERK_SEGMENTS); ImGui::SameLine();
                ImGui::RadioButton("Interpolated moves", &e, CBM_INTERPOLATED_MOVES);
                blendMethod =(cornerBlendMethod)e;
                plan.setCornerBlendMethod(blendMethod);

                if (ImGui::TreeNode("Hard limits per axis")) {
                    ImGui::SeparatorText("Position constraint");
                    showVec3Editor("Pos", &plan.posLimitUpper);

                    ImGui::SeparatorText("Velocity constraint");
                    showVec3Editor("Vel", &plan.velLimit);

                    ImGui::SeparatorText("Acceleration constraint");
                    showVec3Editor("Acc", &plan.accLimit);

                    ImGui::SeparatorText("Jerk constraint");
                    showVec3Editor("Jerk", &plan.jerkLimit);

                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Control points"))
            {

                if (ImGui::Button("All min jerk")) {
                    for (size_t i = 0; i < plan.moves.size(); i++) {
                        scv::move& m = plan.moves[i];
                        m.blendType = CBT_MIN_JERK;
                    }
                } ImGui::SameLine();
                if (ImGui::Button("All max jerk")) {
                    for (size_t i = 0; i < plan.moves.size(); i++) {
                        scv::move& m = plan.moves[i];
                        m.blendType = CBT_MAX_JERK;
                    }
                }

                ImGui::SeparatorText("Tests");

                if (ImGui::Button("Default")) {
                    doRandomizePoints = false;
                    loadTestCase_default();
                } ImGui::SameLine();
                if (ImGui::Button("Straight")) {
                    doRandomizePoints = false;
                    loadTestCase_straight();
                } ImGui::SameLine();
                if (ImGui::Button("Retrace")) {
                    doRandomizePoints = false;
                    loadTestCase_retrace();
                } ImGui::SameLine();
                if (ImGui::Button("Malformed"))
                {
                    doRandomizePoints = false;
                    loadTestCase_malformed();
                } ImGui::SameLine();
                if (ImGui::Button("GCode"))
                {
                    auto gcodeFile = std::filesystem::path("UM3E_3DBenchy.gcode");
                    loadTestCase_file(gcodeFile);
                } ImGui::SameLine();
                if (ImGui::Button("PNP")) {
                    doRandomizePoints = false;
                    loadTestCase_pnp();
                }

                ImGui::Checkbox("Randomize points", &doRandomizePoints);
                ImGui::Checkbox("Stop on violation", &stopOnViolation);

                if (ImGui::TreeNode("Customize points"))
                {
                    if (ImGui::Button("Add point")) {
                        vec3 p = vec3_zero;
                        if ( ! plan.moves.empty() ) {
                            scv::move& el = plan.moves[ plan.moves.size()-1 ];
                            p = el.dst;
                        }
                        scv::move m;
                        m.src = p;
                        m.dst = p + vec3( 2, 2, 2 );
                        m.vel = 10;
                        m.acc = 400;
                        m.jerk = 800;
                        plan.appendMove(m);
                    }

                    if ( ! plan.moves.empty() ) {
                        scv::move& m = plan.moves[0];
                        if (ImGui::TreeNode("Point 0")) {

                            ImGui::SeparatorText("Location");
                            showVec3Editor("Loc 0", &m.src);

                            ImGui::TreePop();
                        }
                    }

                    for (size_t i = 0; i < plan.moves.size(); i++) {
                        scv::move& m = plan.moves[i];
                        snprintf(n, sizeof(n), "Point %d", (int)(i+1));
                        if (ImGui::TreeNode(n)) {

                            ImGui::SeparatorText("Location");
                            snprintf(n, sizeof(n), "Loc %d", (int)(i+1));
                            showVec3Editor(n, &m.dst);
                            ImGui::SeparatorText("Constraints");

                            double tmpf;

                            snprintf(n, sizeof(n), "Vel %d", (int)(i+1));                            
                            tmpf = m.vel;
                            ImGui::InputDouble(n, &tmpf, 0.1f, 1.0f);
                            m.vel = (scv_float)tmpf;

                            snprintf(n, sizeof(n), "Acc %d", (int)(i+1));
                            tmpf = m.acc;
                            ImGui::InputDouble(n, &tmpf, 0.1f, 1.0f);
                            m.acc = (scv_float)tmpf;

                            snprintf(n, sizeof(n), "Jerk %d", (int)(i+1));                            
                            tmpf = m.jerk;
                            ImGui::InputDouble(n, &tmpf, 0.1f, 1.0f);
                            m.jerk = (scv_float)tmpf;

                            if ( i > 0 ) {
                                int e = m.blendType;
                                ImGui::RadioButton("None", &e, CBT_NONE); ImGui::SameLine();
                                ImGui::RadioButton("Min jerk", &e, CBT_MIN_JERK); ImGui::SameLine();
                                ImGui::RadioButton("Max jerk", &e, CBT_MAX_JERK);
                                m.blendType = (cornerBlendType)e;
                            }

                            ImGui::TreePop();
                        }
                    }

                    for (size_t i = 1; i < plan.moves.size(); i++) {
                        scv::move& m = plan.moves[i];
                        scv::move& prevMove = plan.moves[i-1];
                        m.src.x = prevMove.dst.x;
                        m.src.y = prevMove.dst.y;
                        m.src.z = prevMove.dst.z;
                    }

                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Stats"))
            {
                ImGui::Text("Calculation time %.1f us", calcTime);
                ImGui::Text("Violation: %s", haveViolation ? "yes":"no");
                ImGui::Text("Num segments: %d",(int)plan.getSegments().size());
                ImGui::Text("Traverse time: %.2f",plan.getTraverseTime());
                ImGui::Text("Average framerate: %.1f fps)", io.Framerate);
            }

            ImGui::SliderFloat("Max overlap", &maxOverlapFraction, 0, 1);
            showPlots();

            ImGui::End();
        }

        //if (show_demo_window)
        //    ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
