#include "ofxWhisperBufferedSoundInput.h"
#include "whisper.h"

ofxWhisperBufferedSoundInput::ofxWhisperBufferedSoundInput():
_isSetup(0),
_rms(0),
_peak(0),
_numChannels(0)
{

}

ofxWhisperBufferedSoundInput::~ofxWhisperBufferedSoundInput(){
    m_soundStream.stop();
    m_soundStream.close();
}

template<typename T>
static T getMaxValue(const std::vector<T>& values){
    T mx = 0;
    for(const auto & v: values){
        if((v > mx) || (v*-1) > mx){
            mx = (v > 0)?v:(v*-1);
        }
    }
    return mx;
}

ofRectangle ofxWhisperBufferedSoundInput::draw(const ofRectangle& rect){
    
    ofSetColor(0);
    ofFill();
    ofDrawRectangle(rect);
    
    ofSetColor(ofColor::blue);
    float h = rect.height * _rms.load();
    ofDrawRectangle(rect.x, rect.height - h, rect.width, h );
    
    ofSetColor(ofColor:: yellow);
    float y = ofMap(_peak.load(), 0, 1, rect.getMaxY(), rect.getMinY());

    ofSetLineWidth(3);
    ofDrawLine(rect.getMinX(), y, rect.getMaxX(), y);
    
    ofSetColor(200);
    ofNoFill();
    ofDrawRectangle(rect);
    
    ofRectangle r = rect;
    
    stringstream ss;
    
    ss << "rms:\n " << _rms.load() << "\npeak:\n" << _peak.load();
    
    auto x = rect.x;
    y = rect.getMaxY() + 20;
    
    ofDrawBitmapStringHighlight(ss.str(), x, y);
    auto bb = bf.getBoundingBox(ss.str(), x, y);
    r.growToInclude(bb);
    return r;
}

//void ofxWhisperBufferedSoundInput::callback(uint8_t * stream, int len) {
void ofxWhisperBufferedSoundInput::audioIn( ofSoundBuffer& buffer ) {
    if(_isSetup.load()){
         if(ringBuffer){
             if(!sampleRateConverter){
                 sampleRateConverter = make_unique<ofxSamplerate>() ;
             }
             
             _numChannels = buffer.getNumChannels();
             
             ofSoundBuffer converted;
             
             sampleRateConverter->changeSampleRate(buffer, converted, WHISPER_SAMPLE_RATE);
             
             ringBuffer->writeFromBuffer(converted);
             _rms = converted.getRMSAmplitude();
             _peak = getMaxValue(converted.getBuffer());
         }
     }
 }

bool ofxWhisperBufferedSoundInput::hasBufferedMs(uint64_t millis){
    if(ringBuffer && _numChannels.load() > 0){
        
        return (ringBuffer->getNumReadableSamples() >= (_numChannels.load() * WHISPER_SAMPLE_RATE * millis)/1000.0f);
    }
    
    return false;
    
}


bool ofxWhisperBufferedSoundInput::get(ofSoundBuffer& buffer){
    if(!_isSetup.load() || _numChannels.load() == 0) return false;
    
    if(ringBuffer){
        ringBuffer->readIntoBuffer(buffer);
        return true;
    }
    return false;
    
}

//bool ofxWhisperBufferedSoundInput::get(int ms, std::vector<float> & result) {
//    if(!_isSetup.load() || _numChannels.load() == 0) return false;
//    
//    size_t n_samples = _numChannels.load() * WHISPER_SAMPLE_RATE * ms / 1000.0f;
//
//    result.resize(n_samples);
//    
//    if(ringBuffer){
//        ringBuffer->readIntoVector(result);
//        return true;
//    }
//    return false;
//}



bool ofxWhisperBufferedSoundInput::setup(int deviceIndex , int inSampleRate, int bufferSize, int waitDurationMs, ofSoundDevice::Api api){
    
    auto devices = m_soundStream.getDeviceList(api);
        if ( deviceIndex < devices.size()) {
            int i = deviceIndex;
            ofSoundStreamSettings m_soundSettings;
            
            m_soundSettings.setInDevice(devices[i]);
            m_soundSettings.setInListener(this);
            m_soundSettings.numInputChannels = devices[i].inputChannels;
            
            //You can pass 0 as the value for inSampleRate and then the value will be the highest one available in the chosen device
            m_soundSettings.sampleRate = (inSampleRate > 0)?inSampleRate:getMaxValue(devices[i].sampleRates);
            m_soundSettings.numBuffers = 2;
            m_soundSettings.bufferSize = bufferSize;
            
//            ofLogNotice("BufferedAudioInput::setup") << "setting audio device " << i << ": " << devices[i].name << " sampleRate: " << m_soundSettings.sampleRate;
            
            wait_duration = waitDurationMs;
            
            
            
            ringBuffer = make_unique<LockFreeRingBuffer>(m_soundSettings.numInputChannels * WHISPER_SAMPLE_RATE * (waitDurationMs/1000.0f)*5);// make the ring buffer 10 times larger than the input buffers. This size is enough to perform the most commmon sampleRate convertions, etc
            
            m_soundStream.setup(m_soundSettings);
                    
            _isSetup = true;
            return true;
        }
    
    _isSetup = false;
    return false;
}
