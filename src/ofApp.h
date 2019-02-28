/*
The code in this repository is available under the MIT License.

Copyright (c) 2019 - Rafael Redondo

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once

#define SOUND_ON
#define ANIMATIONS_ON
#define LEAP_MOTION_ON

#define SYNAP_DISCHARGE_TIME            800     // ms
#define SYNAP_DISCHARGE_STRENGH         1E-1    // [0.5,5]
#define SYNAP_DISCHARGE_TIME            800     // ms
#define SYNAP_DISCHARGE_STRENGH         1E-1    // [0.5,5]
#define SYNAP_DISCHARGE_DENSITY         1E-4    // [0,1]
#define SYNAP_DISCHARGE_SOUND_DENSITY   0.05    // [0,1]
#define SYNAP_LOCAL_MAX_PERTURBATION    3
#define SYNAP_LOCAL_MAX_NUMBER          2
#define SYNAP_LOCAL_MIN_NUMBER          1
#define SYNAP_MIN_LIFESPAN              15
#define SYNAP_MAX_LIFESPAN              40
#define SYNAP_MAX_SPEED                 4

#define FLATTENING_TIME                 5000
#define INCLUSION_TIME                  4000
#define INCLUSION_SOUND_DENSITY         0.9     // [0,1]
#define NOISE_INCREASE_TIME             6000
#define NOISE_SPEED                     1.12    // [0,...)
#define NOISE_SAMPLING                  5       // 1,2,...

#define SPECTRUM_BANDS                  64
#define SPECTRUM_DECAY                  0.5

#define CAMERA_POSE_ROTATION_SPEED      5E-3
#define CAMERA_POSE_ELEVATION           0.22    // [0,1]
#define CAMERA_POSE_EXCENTRICITY        0.5     // [0,1]

#define CANVAS_INIT_EXTRUSION           -2.6
#define CAMERA_INIT_FOCAL               9000
#define CAMERA_INIT_ZPOS                -1200

#include "ofMain.h"

#ifdef LEAP_MOTION_ON
#define LEAP_MOTION_SENSIBILITY         4E-4    // [0,..)
#include "ofxLeapMotion.h"
#endif

enum Example {
    MENINAS = 0,
    GOYA,
    DEGAS,
    MILLET,
    PICASSO,
    SEURAT,
    PUJOLA1,
    PUJOLA2,
    PUJOLA3,
    VIDEO
};

class Synapse {
    
public:
    
    Synapse(int pos) : position(pos)
    {
        age = lifespan = ofRandom(SYNAP_MIN_LIFESPAN, SYNAP_MAX_LIFESPAN);
        direction = ofPoint( ofRandom(-SYNAP_MAX_SPEED,SYNAP_MAX_SPEED),
                             ofRandom(-SYNAP_MAX_SPEED,SYNAP_MAX_SPEED) );
    }
    ~Synapse(){}
    bool discharge(int width, int height)
    {
        int y = floor(position/width);
        int x = position - y * width;
        x += round( direction.x * ofRandom(0.5,1) );
        y += round( direction.y * ofRandom(0.5,1) );
        
        --lifespan;
        
        if ( x < 1 || y < 1 || x >= width-1 || y >= height-1) lifespan = 0;
        else position = y * width + x;

        return died();
    }
    bool died() { return lifespan <= 0; }
    int getPosition() { return position; }
    float getLifeFactor() { return lifespan / age; }
    float getLifeFactorInv() { return 1 - getLifeFactor(); }

private:
    
    float age;
    int lifespan, position;
    ofPoint direction;
};

class ofApp : public ofBaseApp
{
public:
    
    void setup();
    void update();
    void draw();
    
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    void exit();
    
    void loadExample(Example example);
    void fireNoise();
    void fireSynapses();
    void fireInclusion();
    void fireFlattening();
    void updateNoise();
    void updateSynapses();
    void updateInclusion();
    void updateFlattening();
    void updateCanvas(bool reset = false);
    
    class Canvas : public ofMesh
    {
        public:
        vector<glm::vec3> vertexes;
        vector<ofColor> colors;
        int width, height;
        ofPolyRenderMode render;
        struct Limits {
            glm::vec3 far, near;
        } limits;
        
        void clear() { colors.clear(); vertexes.clear(); ofMesh::clear(); }
        
    } canvas;

    void resetCamera();
    void updatePose();

    class Camera : public ofEasyCam
    {
        public:
        glm::vec3 speed;
        float focal, extrusion;
        bool orbit;
        
    } camera;

    ofVideoPlayer video, video_depth;
    ofImage image, image_depth;

    bool bConsole = true, bVideo = false, bLoaded = false, bDepth = false;
    
    vector<Synapse> synapses;
    float global_discharge_strengh;
    float flattening = 0;
    float noise_starttime = -1E9, firing_starttime = -1E9, flattening_starttime = -1E9, inclusion_starttime = -1E9;
    bool bFlat = false, bInclusion = true, bNoise = false;
    size_t nFirings;

    ofColor central_color, edge_color;

    ofSoundPlayer soundtrack, sounddischarge, soundgrain;
    vector<float> power;
    float spectrum;
    void setupAudio();
    void playGrain();

#ifdef LEAP_MOTION_ON
    size_t hand_id, finger_id;
    ofxLeapMotion leap;
    vector<ofxLeapMotionSimpleHand> hands;
#endif
};
