#include <string>
#include <map>

#include <SFML/Graphics/Shader.hpp>

#include "shaders.hpp"

#define S(...) #__VA_ARGS__

static std::string hsvtorgb_glsl = S(
    vec4 hsvtorgb(vec4 colo)
    {
        vec4 outp;
        float r, g, b, h, s, v;
        r=g=b=h=s=v=0.0;
        h = colo.x; s = colo.y; v = colo.z;
        if (s == 0.0) { r = g = b = v; }
        else {
            float H = mod(floor(h/60.0) , 6.0);
            float p, q, t, f = h/60.0 - H;
            p = v * (1.0 - s);
            q = v * (1.0 - f*s);
            t = v * (1.0 - (1.0 - f)*s);
            if(H == 6.0 || H == 0.0) { r = v; g = t; b = p; }
            else if(H == -1.0 || H == 5.0) { r = v; g = p; b = q; } 
            else if(H == 1.0) { r = q; g = v; b = p; }
            else if(H == 2.0) { r = p; g = v; b = t; }
            else if(H == 3.0) { r = p; g = q; b = v; }
            else if(H == 4.0) { r = t; g = p; b = v; }
        }
        return vec4(r, g, b, colo.w);
    }
);

static std::string atan2_glsl = S(
    float M_PI = 3.1415926535897932;
    float M_PI_2 = 1.5707963267948966;
    float atan2(float x, float y)
    {
       if (x>0.0) { return atan(y/x); }
       else if(x<0.0 && y>0.0) { return atan(y/x) + M_PI; }
       else if(x<0.0 && y<=0.0 ) { return atan(y/x) - M_PI; }
       else if(x==0.0 && y>0.0 ) { return M_PI_2; }
       else if(x==0.0 && y<0.0 ) { return -M_PI_2; }
       return 0.0;
    }
);

static std::string defaultVertex = S(
    void main()
    {
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
        gl_FrontColor = gl_Color;
    }
);
static std::string defaultFragment = S(
    uniform sampler2D texture;
    uniform float scale;
    uniform float bias;

    void main()
    {
        vec3 p = texture2D(texture, gl_TexCoord[0].xy).xyz;
        gl_FragColor = vec4(clamp(p * scale + bias, 0.0, 1.0), 1.0);
    }
);

static std::string grayVertex = defaultVertex;
static std::string grayFragment = S(
    uniform sampler2D texture;
    uniform float scale;
    uniform float bias;

    void main()
    {
        vec4 p = vec4(texture2D(texture, gl_TexCoord[0].xy).x);
        gl_FragColor = clamp(p * scale + bias, 0.0, 1.0);
    }
);

// from https://github.com/gfacciol/pvflip
static std::string opticalFlowVertex = defaultVertex;
static std::string opticalFlowFragment = hsvtorgb_glsl + atan2_glsl + S(
    uniform sampler2D texture;
    uniform float scale;
    uniform float bias;

    void main (void)
    {
        vec4 p = texture2D(texture, gl_TexCoord[0].xy);
        float a = (180.0/M_PI)*(atan2(-p.x,p.w) + M_PI);
        float r = sqrt(p.x*p.x+p.w*p.w) * scale + bias;
        vec4 q = vec4(a, clamp(r,0.0,1.0),clamp(r,0.0,1.0),0.0);

        gl_FragColor = clamp(hsvtorgb(q), 0.0, 1.0);
    }
);

static std::map<std::string, std::string[2]> shaderCodes = {
    {"default", {defaultVertex, defaultFragment}},
    {"gray", {grayVertex, grayFragment}},
    {"opticalFlow", {opticalFlowVertex, opticalFlowFragment}},
};

std::map<std::string, sf::Shader> gShaders;

void loadShaders()
{
    for (auto& it : shaderCodes) {
        gShaders[it.first].loadFromMemory(it.second[0], it.second[1]);
    }
}

