
#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    ofSetLogLevel(OF_LOG_NOTICE);
    ofSetVerticalSync(true);
    ofFill();
    ofSetFrameRate(30);
    glPointSize(1);
    canvas.render = OF_MESH_WIREFRAME;
    
#ifdef LEAP_MOTION_ON
    hand_id = 0;    // 0-righthand 1-lefthand;
    finger_id = 2;
    leap.open();
    leap.setMappingX(-100, 100, -ofGetScreenWidth()  / 2, ofGetScreenWidth()  / 2);
    leap.setMappingY(  50, 100, -ofGetScreenHeight() / 2, ofGetScreenHeight() / 2);
    leap.setMappingZ(   0, 100, -ofGetScreenWidth()  / 2, ofGetScreenWidth()  / 2);
#endif
    
    setupAudio();
    resetCamera();
    loadExample(MENINAS);
}

//--------------------------------------------------------------
void ofApp::update()
{
    camera.move(camera.speed);
    
    if ( bVideo && video.isPlaying() )
    {
        video.update();
        video_depth.update();
        updateCanvas();
    }
    
#ifdef ANIMATIONS_ON
    updateSynapses();
    updateFlattening();
    updateInclusion();
    updateNoise();
    updatePose();
#endif
    
#ifdef SOUND_ON
    float s = 0;
    float * band = ofSoundGetSpectrum(SPECTRUM_BANDS);
    for (size_t b = 0; b < SPECTRUM_BANDS; ++b) s += band[b];
    if (power.size() > ofGetFrameRate() * SPECTRUM_DECAY ) power.erase(power.begin());
    power.push_back(s);
    spectrum = std::accumulate(power.begin(), power.end(), 0.f) / (float) power.size();
#endif
        
#ifdef LEAP_MOTION_ON
    hands = leap.getSimpleHands();
    if ( leap.isFrameNew() && hands.size() )
    {
        if ( hands[hand_id].fingers.size() >= finger_id )
        {
            ofVec3f finger_orientation = hands[hand_id].handPos - hands[hand_id].fingers[finger_id].pos;
            camera.tiltDeg( -finger_orientation.y * LEAP_MOTION_SENSIBILITY );
            camera.panDeg(  -finger_orientation.x * LEAP_MOTION_SENSIBILITY );
            camera.setPosition( camera.getPosition() - camera.getLookAtDir() * hands[hand_id].handPos.z * LEAP_MOTION_SENSIBILITY * 5 );
            camera.rollDeg( ( hands[hand_id].handPos.y - hands[hand_id].fingers[0].pos.y ) * LEAP_MOTION_SENSIBILITY);
        }
    }
    leap.markFrameAsOld();
#endif
}

//--------------------------------------------------------------
void ofApp::draw()
{
    ofDisableDepthTest();
    ofBackgroundGradient(central_color, edge_color, OF_GRADIENT_CIRCULAR);
    //ofBackground(central_color * 0.6 - edge_color * 0.4);
    ofEnableDepthTest();
    camera.begin();
    canvas.draw(canvas.render);
    camera.end();
    ofDisableDepthTest();
    
    if ( ! bConsole ) return;
    
    ofPushStyle();
    ofSetColor(255);
    string msg = "Source " + ofToString(bVideo ? video.getWidth() : image.getWidth()) + " x "
                           + ofToString(bVideo ? video.getHeight() : image.getHeight());
    msg += "\nFps: "                    + ofToString(ofGetFrameRate(), 2);
    msg += "\nCamera position: "        + ofToString(camera.getPosition(), 2);
    msg += "\nCamera Speed 'arrows': "  + ofToString(camera.speed, 2);
    msg += "\nCamera reset 'return'";
    msg += "\nCamera orbit 'spacebar'";
    msg += "\nFocal distance 'q,w': "   + ofToString(camera.focal, 2);
    msg += "\nExtrusion 'e,r': "        + ofToString(camera.extrusion, 2);
    msg += "\nSpectrum: "               + ofToString(spectrum, 2);
    msg += "\nRender mode 'z,x,c'";
    msg += "\nShow depth 'd'";
    msg += "\nPlay soundtrack '.'";
    msg += "\nFirings 's' 'f' 'i' 'n'";
#ifdef LEAP_MOTION_ON
    msg += leap.isConnected() ? "\nLEAP connected!" : "\nLEAP disconnected o_0";
#endif
    msg += "\nHelp 'h'";
    ofDrawBitmapString(msg, 10, 10);
    ofSetColor(255,0,0);
    if ( ! bLoaded ) ofDrawBitmapString("Error loading sources o_O", 10, ofGetWindowHeight()-20);
    ofPopStyle();
}
//--------------------------------------------------------------
void ofApp::fireSynapses()
{
    ofLogNotice() << "Firing! " << ofGetFrameNum();
    
    float mFirings = canvas.width * canvas.height * SYNAP_DISCHARGE_DENSITY;
    nFirings = ofRandom(mFirings * 0.3, mFirings * 2.f);
    for (size_t n = 0; n < nFirings; ++n)
    {
        int pos = round( ofRandom(canvas.width * canvas.height - 1) );
        size_t nLocalFirings = round(ofRandom(SYNAP_LOCAL_MIN_NUMBER-0.5, SYNAP_LOCAL_MAX_NUMBER+0.5));
        for (size_t t = 0; t < nLocalFirings; ++t)
            synapses.push_back( Synapse(pos) );
    }
    global_discharge_strengh = ofRandom(0.01, SYNAP_DISCHARGE_STRENGH);
    firing_starttime = ofGetElapsedTimeMillis();
    nFirings = synapses.size();
    
#ifdef SOUND_ON
    sounddischarge.setSpeed( ofMap(1 - global_discharge_strengh / SYNAP_DISCHARGE_STRENGH, 0, 1, 0.8, 1.2) );
    sounddischarge.setPosition(ofRandomuf());
    sounddischarge.play();
#endif
}
void ofApp::updateSynapses()
{
    // Mesh pointers
    auto pColors = canvas.getColorsPointer();
    auto pVertexes = canvas.getVerticesPointer();
    
    // Canvas: global perturbation
    float thickness = canvas.limits.far.z - canvas.limits.near.z;
    float elapsed_time = ofGetElapsedTimeMillis() - firing_starttime;
    if ( elapsed_time < SYNAP_DISCHARGE_TIME )
    {
        float inc = elapsed_time / (float) SYNAP_DISCHARGE_TIME;
        float wave_z =  canvas.limits.far.z - thickness * inc;
        // inc = inc < 0.1 ? inc / 0.1 : (1 - inc) / 0.9;   // Fast-in, easy-out
        for (size_t pos = 0; pos < canvas.vertexes.size(); ++pos)
        {
            float wave_phase = 1.5 - abs(canvas.vertexes[pos].z - wave_z) / thickness;
            wave_phase = pow(wave_phase,6);
            *(pVertexes+pos) = canvas.vertexes[pos]
                             + (1-wave_phase) * global_discharge_strengh * glm::vec3(ofRandom(-1,1),ofRandom(-1,1),ofRandom(-1,1));
                            // + inc * global_discharge_strengh * glm::vec3(ofRandom(-1,1),ofRandom(-1,1),ofRandom(-1,1));
        }
    }
    
    if ( ! synapses.size() ) return;

    // Calm down dead neurons and discharge the others
    for (auto & s : synapses)
    {
        int pos = s.getPosition();
        *(pColors+pos) = canvas.colors[pos];
        *(pVertexes+pos) = canvas.vertexes[pos];
    }
    
    // Remove dead neurons
    synapses.erase(std::remove_if(synapses.begin(), synapses.end(),
                                  [this](Synapse & s) { return s.discharge(canvas.width, canvas.height); }), synapses.end());
    
    // Local perturbation discharge
    for (auto & s : synapses)
    {
        int pos = s.getPosition();
        
        // Color
        float brightness = (pColors+pos)->getBrightness();
        float inc = (pColors+pos)->limit() - brightness;
        (pColors+pos)->setBrightness( brightness + inc * 3.0 * s.getLifeFactor());
        (pColors+pos)->setSaturation( (pColors+pos)->getSaturation() * s.getLifeFactorInv() );

        // 3D Position
        float n = ofGetElapsedTimef();
        glm::vec3 perturbation( ofSignedNoise(n+pos+10),
                                ofSignedNoise(n+pos+20),
                                ofSignedNoise(n+pos+30));
        *(pVertexes+pos) += perturbation * SYNAP_LOCAL_MAX_PERTURBATION;// * s.getLifeFactor();
        
#ifdef SOUND_ON
        if ( ofRandomuf() < SYNAP_DISCHARGE_SOUND_DENSITY ) playGrain();
#endif
    }

    if ( ! synapses.size() ) ofLogNotice() << "Fire off " << ofGetFrameNum();
}
void ofApp::fireFlattening()
{
    bFlat = ! bFlat;
    flattening_starttime = ofGetElapsedTimeMillis();
}
void ofApp::updateFlattening()
{
    float elapsed_time = ofGetElapsedTimeMillis() - flattening_starttime;
    flattening =  bFlat ? elapsed_time / (float) FLATTENING_TIME : 1 - elapsed_time / (float) FLATTENING_TIME;
    if ( flattening < 0 || flattening > 1 ) { flattening = MIN(MAX(flattening, 0.f), 1.f); return; }
    flattening *= flattening;
    float middle = (canvas.limits.far.z - canvas.limits.near.z) * 0.5 + canvas.limits.near.z;
    auto pVertexes = canvas.getVerticesPointer();
    for (size_t pos = 0; pos < canvas.vertexes.size(); ++pos)
        (pVertexes+pos)->z = flattening * middle + (1 - flattening) * canvas.vertexes[pos].z;
}
void ofApp::fireInclusion()
{
    bInclusion = ! bInclusion;
    inclusion_starttime = ofGetElapsedTimeMillis();
}
void ofApp::updateInclusion()
{
    float elapsed_time = ofGetElapsedTimeMillis() - inclusion_starttime;
    if ( elapsed_time > INCLUSION_TIME ) return;
    float inclusion = bInclusion ? elapsed_time / (float) INCLUSION_TIME : 1 - elapsed_time / (float) INCLUSION_TIME;
    // inclusion *= 1 + 0.2 * ofNoise(4 * ofGetElapsedTimeMillis()) * (1-inclusion);
    float cut = (canvas.limits.far.z - canvas.limits.near.z + 4) * inclusion + canvas.limits.near.z - 2;
    auto pColors = canvas.getColorsPointer();
    for (size_t pos = 0; pos < canvas.vertexes.size(); ++pos)
        (pColors+pos)->a = canvas.vertexes[pos].z > cut ? 1.f : 0.f;
    
#ifdef SOUND_ON
    if ( ofRandomuf() < INCLUSION_SOUND_DENSITY ) playGrain();
#endif
}
void ofApp::fireNoise()
{
    bNoise = ! bNoise;
    noise_starttime = ofGetElapsedTimeMillis();
}
void ofApp::updateNoise()
{
    float elapsed_time = ofGetElapsedTimeMillis() - noise_starttime;
    float noise = bNoise ? elapsed_time / (float) NOISE_INCREASE_TIME : 1 - elapsed_time / (float) NOISE_INCREASE_TIME;
    if (noise < 0) return;
    noise = MIN(noise, 1.f);
    int s = NOISE_SAMPLING;
    glm::vec2 ss(canvas.width - 2 * s - 1, canvas.height - 2 * s - 1);
    float speed = NOISE_SPEED;
    float n = ofGetElapsedTimef();
#ifdef SOUND_ON
    float nn = spectrum * 15.f + 0.5;
#else
    float nn = 8 * ofNoise( n ) + 1;
#endif
    float thick = (canvas.limits.far.z - canvas.limits.near.z);
    float middle = 0.5 * thick + canvas.limits.near.z;
    auto pVertexes = canvas.getVerticesPointer();
    for ( int y = 0; y < canvas.height; y += s )
    {
        float Y = y * canvas.width;
        for ( int x = 0; x < canvas.width; x += s )
        {
            float N = 10 * nn * ofNoise(speed * n + x + Y);
            int yextra = ss.y > y ? 0 : canvas.height - y - s;
            int xextra = ss.x > x ? 0 : canvas.width  - x - s;
            for ( int yy = y; yy < y + s + yextra; ++yy )
            {
                int ys = yy * canvas.width;
                for ( int xx = x; xx < x + s + xextra ; ++xx )
                {
                    int pos = xx + ys;
                    float strength = (canvas.limits.far.z - canvas.vertexes[pos].z) / thick;
                    strength *= strength;
                    float inc = N * strength + ofRandom(nn * strength + 1);
                    if ( flattening >= 1 ) (pVertexes + pos)->z = middle - inc * noise;
                    else                   (pVertexes + pos)->z -= inc * pow( noise * flattening, 0.5);
                }
            }
        }
    }
}
void ofApp::updateCanvas(bool reset)
{
    if ( ! bLoaded ) return;
    
    // Get pixels
    if ( ! (bVideo ? video.getPixels().isAllocated() : image.getPixels().isAllocated()) )               return;
    if ( ! (bVideo ? video_depth.getPixels().isAllocated() : image_depth.getPixels().isAllocated()) )   return;

    unsigned char * iData = bVideo ? video.getPixels().getData() : image.getPixels().getData();
    unsigned char * dData = bVideo ? video_depth.getPixels().getData() : image_depth.getPixels().getData();
    
    // Reset mesh
    if ( ! reset )
    {
        canvas.clearColors();
        canvas.clearVertices();
        canvas.vertexes.clear();
        canvas.colors.clear();

    } else canvas.clear();
    
    glm::vec2 centering = glm::vec2( canvas.width  * 0.5, canvas.height * 0.5 );

    // Update mesh
    for ( int y = 0; y < canvas.height; ++y )
    {
        for ( int x = 0; x < canvas.width; ++x )
        {
            int pos = x + y * canvas.width;
            float d = 255 - dData[ bVideo ? pos * 3 : pos ];

            // Color
            pos *= 3;
            int r = iData[ pos ];
            int g = iData[ pos + 1 ];
            int b = iData[ pos + 2 ];
            int a = 255.f;
            ofColor color = bDepth ? ofColor(255-d,255-d,255-d) : ofColor(r,g,b,a);
            
            // 3D Location
            float w = x - centering.x;
            float h = y - centering.y;
            ofVec3f v(w,h,-camera.focal);
            float m = v.length();
            v /= m; // v.normalize();
            v *= ( m + d * camera.extrusion ) - ofVec3f(0,0,camera.focal);
            
            canvas.addVertex(v);
            canvas.addColor(color);
            
            // Store depth and color
            canvas.vertexes.push_back(v);
            canvas.colors.push_back(color);

            if ( ! reset ) continue;
            if ( x == canvas.width-1 || y == canvas.height-1 ) continue;
            
            canvas.addIndex(x     + y     * canvas.width);
            canvas.addIndex((x+1) + y     * canvas.width);
            canvas.addIndex(x     + (y+1) * canvas.width);
            
            if (canvas.render != OF_MESH_FILL) continue;
            
            canvas.addIndex((x+1) + y     * canvas.width);
            canvas.addIndex((x+1) + (y+1) * canvas.width);
            canvas.addIndex(x     + (y+1) * canvas.width);
        }
    }
    canvas.limits.far = *std::max_element(canvas.vertexes.begin(), canvas.vertexes.end(), [](glm::vec3 v1,glm::vec3 v2){return v1.z<v2.z;});
    canvas.limits.near= *std::min_element(canvas.vertexes.begin(), canvas.vertexes.end(), [](glm::vec3 v1,glm::vec3 v2){return v1.z<v2.z;});
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    switch (key)
    {
        case 's': fireSynapses();                                           break;
        case 'f': fireFlattening();                                         break;
        case 'i': fireInclusion();                                          break;
        case 'n': fireNoise();                                              break;
        case 'd': bDepth = ! bDepth; updateCanvas();                        break;
        case 'z': canvas.render = OF_MESH_POINTS;    updateCanvas(true);    break;
        case 'x': canvas.render = OF_MESH_WIREFRAME; updateCanvas(true);    break;
        case 'c': canvas.render = OF_MESH_FILL;      updateCanvas(true);    break;
        case 'e': camera.extrusion += 0.1;           updateCanvas();        break;
        case 'r': camera.extrusion -= 0.1;           updateCanvas();        break;
        case 'q': camera.focal += 500;               updateCanvas();        break;
        case 'w': camera.focal -= 500;               updateCanvas();        break;
        case 'h': bConsole = !bConsole;                                     break;
        case '1': loadExample(MENINAS);                                     break;
        case '2': loadExample(GOYA);                                        break;
        case '3': loadExample(DEGAS);                                       break;
        case '4': loadExample(MILLET);                                      break;
        case '5': loadExample(PICASSO);                                     break;
        case '6': loadExample(SEURAT);                                      break;
        case '7': loadExample(PUJOLA1);                                     break;
        case '8': loadExample(PUJOLA2);                                     break;
        case '9': loadExample(PUJOLA3);                                     break;
        case '0': loadExample(VIDEO);                                       break;
        case '.': soundtrack.setPosition(0); soundtrack.play();             break;
        case ' ': camera.orbit = ! camera.orbit;                            break;
        case OF_KEY_RETURN:     resetCamera();                              break;
        case OF_KEY_RIGHT:      camera.speed.x += 0.1;                      break;
        case OF_KEY_LEFT:       camera.speed.x -= 0.1;                      break;
        case OF_KEY_UP:
            if (ofGetKeyPressed(OF_KEY_SHIFT))   camera.speed.z += 0.1;
            else                                 camera.speed.y -= 0.1;
            break;
        case OF_KEY_DOWN:
            if (ofGetKeyPressed(OF_KEY_SHIFT))   camera.speed.z -= 0.1;
            else                                 camera.speed.y += 0.1;
            break;
        default: break;
    }
}
void ofApp::loadExample(Example example)
{
    string image_name, depth_name;
    bool request_video = false;
    
    switch (example) {
            
        case MENINAS:
            image_name = "velazquez_meninas.jpg";
            depth_name = "velazquez_meninas_depth.png";
            break;
        case GOYA:
            image_name = "goya_mayo.png";
            depth_name = "goya_mayo_depth.png";
            break;
        case DEGAS:
            image_name = "degas_dancers.png";
            depth_name = "degas_dancers_depth.png";
            break;
        case MILLET:
            image_name = "millet_angelus.png";
            depth_name = "millet_angelus_depth.png";
            break;
        case PICASSO:
            image_name = "picasso_azul.png";
            depth_name = "picasso_azul_depth.png";
            break;
        case SEURAT:
            image_name = "seurat.png";
            depth_name = "seurat_depth.png";
            break;
        case PUJOLA1:
            image_name = "pujola1.png";
            depth_name = "pujola1_depth.png";
            break;
        case PUJOLA2:
            image_name = "pujola2.png";
            depth_name = "pujola2_depth.png";
            break;
        case PUJOLA3:
            image_name = "pujola3.png";
            depth_name = "pujola3_depth.png";
            break;
        case VIDEO:
            image_name = "eurecat_indoor_small.mov";
            depth_name = "depth_indoor_small.mov";
            request_video = true;
            break;
        default:
            break;
    }
    
    if (request_video)
    {
        bLoaded = video.load(image_name);
        bLoaded &= video_depth.load(depth_name);
        
        if ( ! bLoaded ) { ofLogError() << "Resource not found"; return; }
        
        video.setVolume(0);
        video_depth.setVolume(0);
        video.play();
        video_depth.play();
        
        canvas.width = video.getWidth();
        canvas.height = video.getHeight();
        
        central_color = edge_color = ofColor(0, 0, 0);
        bVideo = true;

    } else {
        
        bLoaded = image.load(image_name);
        bLoaded &= image_depth.load(depth_name);
        
        if ( ! bLoaded ) { ofLogError() << "Resource not found"; return; }

        // image.resize(image.getWidth() * 0.5, image.getHeight() * 0.5);
        image_depth.resize(image.getWidth(), image.getHeight());
        image_depth.setImageType(OF_IMAGE_GRAYSCALE);
        image.setImageType(OF_IMAGE_COLOR);
        
        canvas.width = image.getWidth();
        canvas.height = image.getHeight();

        central_color = edge_color = image.getPixels().getColor( canvas.width * ( 1 + canvas.height * 0.5 ) );
        central_color.setBrightness(55);
        edge_color.setBrightness(15);
        
        bVideo = false;
    }

    bFlat = bNoise =  bInclusion = bDepth = false;
    updateCanvas(true);
}
void ofApp::updatePose()
{
    if (!camera.orbit) return;

    float travel = ofMap(flattening, 0, 1, 0.8, 1.0);
    glm::vec3 up( 0, 0, 1);
    glm::vec3 center(0, 0, (canvas.limits.far.z - canvas.limits.near.z) * 0.5 * travel + canvas.limits.near.z);
    float angle = ofGetFrameNum() * CAMERA_POSE_ROTATION_SPEED;
    float elevation = center.z * (0.8 - CAMERA_POSE_ELEVATION * abs(cos(angle)));
    glm::vec3 origin( center.x, center.y, elevation );
    glm::vec3 excentricity = glm::rotate(glm::vec3(1,0,0), angle, glm::vec3(0,0,1));
    glm::vec3 position = origin + excentricity * CAMERA_POSE_EXCENTRICITY * MIN(canvas.width,canvas.height);
    camera.setPosition(position);
    camera.lookAt(center,up);
}
void ofApp::resetCamera()
{
    camera.orbit = 0;
    camera.speed = glm::vec3(0.f,0.f,0.f);
    camera.extrusion = CANVAS_INIT_EXTRUSION;
    camera.focal = CAMERA_INIT_FOCAL;
    camera.setScale(-1,-1, 1);
    camera.setFov(70);
    camera.setPosition(glm::vec3( 0., 0., CAMERA_INIT_ZPOS ));
    glm::vec3 center(0, 0, (canvas.limits.far.z - canvas.limits.near.z) * 0.5 + canvas.limits.near.z);
    glm::vec3 up(0,1,0);
    camera.lookAt(center,up);
}
//--------------------------------------------------------------
void ofApp::setupAudio()
{
#ifndef SOUND_ON
    return;
#endif
    // Players OST and FX
    soundtrack.load("ClairDeLune_ROLI.wav");
    soundtrack.setVolume(1.f);
    //soundtrack.play();
    sounddischarge.load("SignalInterference29cut.wav");
    sounddischarge.setVolume(0.6);
    sounddischarge.play();
    soundgrain.load("34170__glaneur-de-sons__electric-wire-03_cut.wav");
    soundgrain.setVolume(0.6);
    soundgrain.setMultiPlay(true);
}
void ofApp::playGrain()
{
    soundgrain.setVolume( 1.0 );
    soundgrain.setPan( ofRandomf() );
    soundgrain.setPosition( 0.6 * ofRandomuf() );
    soundgrain.setSpeed( ofMap( ofRandomuf(), 0, 1, 0.7, 4) );
    soundgrain.play();
    soundgrain.setPan( ofRandomf() );
    soundgrain.setSpeed( ofMap( ofRandomuf(), 0, 1, 4, 20) );
    soundgrain.play();
}
//--------------------------------------------------------------
void ofApp::exit()
{
    video.close();
    video_depth.close();
#ifdef LEAP_MOTION
    leap.close();
#endif
}
//------------------------------------------------------------
void ofApp::keyReleased(int key){}
void ofApp::mouseMoved(int x, int y){}
void ofApp::mouseDragged(int x, int y, int button){}
void ofApp::mousePressed(int x, int y, int button){}
void ofApp::mouseReleased(int x, int y, int button){}
void ofApp::mouseEntered(int x, int y){}
void ofApp::mouseExited(int x, int y){}
void ofApp::windowResized(int w, int h){}
void ofApp::gotMessage(ofMessage msg){}
void ofApp::dragEvent(ofDragInfo dragInfo){}
