#include <string>
#include <map>
#include <array>

#include "Shader.hpp"
#include "shaders.hpp"
#include "globals.hpp"

#define S(...) #__VA_ARGS__

static std::string scalemap = S(
    uniform vec3 scale;
    uniform vec3 bias;

    float scalemap(float p) {
        return clamp(p * scale.x + bias.x, 0.0, 1.0);
    }
    vec2 scalemap(vec2 p) {
        return clamp(p * scale.xy + bias.xy, 0.0, 1.0);
    }
    vec3 scalemap(vec3 p) {
        return clamp(p * scale.xyz + bias.xyz, 0.0, 1.0);
    }
);

static std::string hsvtorgb_glsl = S(
\nvec3 hsvtorgb(vec3 colo)
\n{
\n    vec4 outp;
\n    float r, g, b, h, s, v;
\n    r=g=b=h=s=v=0.0;
\n    h = colo.x; s = colo.y; v = colo.z;
\n    if (s == 0.0) { r = g = b = v; }
\n    else {
\n        float H = mod(floor(h/60.0) , 6.0);
\n        float p, q, t, f = h/60.0 - H;
\n        p = v * (1.0 - s);
\n        q = v * (1.0 - f*s);
\n        t = v * (1.0 - (1.0 - f)*s);
\n        if(H == 6.0 || H == 0.0) { r = v; g = t; b = p; }
\n        else if(H == -1.0 || H == 5.0) { r = v; g = p; b = q; } 
\n        else if(H == 1.0) { r = q; g = v; b = p; }
\n        else if(H == 2.0) { r = p; g = v; b = t; }
\n        else if(H == 3.0) { r = p; g = q; b = v; }
\n        else if(H == 4.0) { r = t; g = p; b = v; }
\n    }
\n    return vec3(r, g, b);
\n}
);

static std::string atan2_glsl = S(
\nfloat M_PI = 3.1415926535897932;
\nfloat M_PI_2 = 1.5707963267948966;
\nfloat atan2(float x, float y)
\n{
\n   if (x>0.0) { return atan(y/x); }
\n   else if(x<0.0 && y>0.0) { return atan(y/x) + M_PI; }
\n   else if(x<0.0 && y<=0.0 ) { return atan(y/x) - M_PI; }
\n   else if(x==0.0 && y>0.0 ) { return M_PI_2; }
\n   else if(x==0.0 && y<0.0 ) { return -M_PI_2; }
\n   return 0.0;
\n}
);

static std::string defaultVertex = S(
    void main()
    {
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
        gl_FrontColor = gl_Color;
    }
);

static std::string mainFragment = scalemap + S(
    uniform sampler2D texture;

    void main()
    {
        vec4 border = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 p = texture2D(texture, gl_TexCoord[0].xy);
        if (gl_TexCoord[0].x < 0.0 || gl_TexCoord[0].x > 1.0) {
            gl_FragColor = border;
        } else if (gl_TexCoord[0].y < 0.0 || gl_TexCoord[0].y > 1.0) {
            gl_FragColor = border;
        } else {
            gl_FragColor = vec4(tonemap(scalemap(p.rgb)), 1.0);
        }
    }
);

static std::string rgbTonemap = S(
\nvec3 tonemap(vec3 p)
\n{
\n    return p;
\n}
);

static std::string grayTonemap = S(
\nvec3 tonemap(vec3 p)
\n{
\n    return vec3(p.x);
\n}
);

// from https://github.com/gfacciol/pvflip
static std::string opticalFlowTonemap = hsvtorgb_glsl + atan2_glsl + S(
\nvec3 tonemap(vec3 p)
\n{
\n    float a = (180.0/M_PI)*(atan2(-p.x, p.y) + M_PI);
\n    float r = sqrt(p.x*p.x+p.y*p.y);
\n    vec3 q = vec3(a, r, r);
\n    return hsvtorgb(q);
\n}
);

// from https://github.com/gfacciol/pvflip
static std::string jetTonemap =  S(
\nvec3 tonemap(vec3 q)
\n{
\n    float d = q.x;
\n    if(d < 0.0) d = -0.05;
\n    if(d > 1.0) d =  1.05;
\n    d = d/1.15 + 0.1;
\n    vec3 p;
\n    p.x = 1.5 - abs(d - .75)*4.0;
\n    p.y = 1.5 - abs(d - .50)*4.0;
\n    p.z = 1.5 - abs(d - .25)*4.0;
\n    return p;
\n}
);

bool loadShader(const std::string& name, const std::string& tonemap)
{
    Shader* shader = new Shader;
    shader->name = name;
    std::copy(tonemap.begin(), tonemap.end()+1, shader->codeTonemap);
    std::copy(mainFragment.begin(), mainFragment.end()+1, shader->codeFragment);
    std::copy(defaultVertex.begin(), defaultVertex.end()+1, shader->codeVertex);
    if (shader->compile() || 1) {
        gShaders.push_back(shader);
        return true;
    }
    return false;
}

void loadDefaultShaders()
{
    loadShader("default", rgbTonemap);
    loadShader("gray", grayTonemap);
    loadShader("opticalFlow", opticalFlowTonemap);
    loadShader("jet", jetTonemap);
}

Shader* getShader(const std::string& name)
{
    for (auto s : gShaders) {
        if (s->name == name)
            return s;
    }
    return nullptr;
}

