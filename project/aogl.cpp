#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <stack>

#include <cmath>

#include "glew/glew.h"

#include "GLFW/glfw3.h"
#include "stb/stb_image.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw_gl3.h"

#include "glm/glm.hpp"
#include "glm/vec3.hpp" // glm::vec3
#include "glm/vec4.hpp" // glm::vec4, glm::ivec4
#include "glm/mat4x4.hpp" // glm::mat4
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/type_ptr.hpp" // glm::value_ptr
#include "glm/ext.hpp"

/* assimp include files. These three are usually needed. */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif

#if DEBUG_PRINT == 0
#define debug_print(FORMAT, ...) ((void)0)
#else
#ifdef _MSC_VER
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __func__, __FILE__, __LINE__, __VA_ARGS__)
#endif
#endif

// Font buffers
extern const unsigned char DroidSans_ttf[];
extern const unsigned int DroidSans_ttf_len;    

// Shader utils
int check_link_error(GLuint program);
int check_compile_error(GLuint shader, const char ** sourceBuffer);
int check_link_error(GLuint program);
GLuint compile_shader(GLenum shaderType, const char * sourceBuffer, int bufferSize);
GLuint compile_shader_from_file(GLenum shaderType, const char * fileName);

// OpenGL utils
bool checkError(const char* title);

struct Camera
{
    float radius;
    float theta;
    float phi;
    glm::vec3 o;
    glm::vec3 eye;
    glm::vec3 up;
};
void camera_defaults(Camera & c);
void camera_zoom(Camera & c, float factor);
void camera_turn(Camera & c, float phi, float theta);
void camera_pan(Camera & c, float x, float y);

struct GUIStates
{
    bool panLock;
    bool turnLock;
    bool zoomLock;
    int lockPositionX;
    int lockPositionY;
    int camera;
    double time;
    bool playing;
    int scroll;
    bool display;
    bool blit;
    static const float MOUSE_PAN_SPEED;
    static const float MOUSE_ZOOM_SPEED;
    static const float MOUSE_TURN_SPEED;
};
const float GUIStates::MOUSE_PAN_SPEED = 0.001f;
const float GUIStates::MOUSE_ZOOM_SPEED = 0.05f;
const float GUIStates::MOUSE_TURN_SPEED = 0.005f;
void init_gui_states(GUIStates & guiStates);



struct SpotLight
{
    glm::vec3 position;
    float angle;
    glm::vec3 direction;
    float penumbraAngle;
    glm::vec3 color;
    float intensity;
    glm::mat4 worldToLightScreen;
};

int main( int argc, char **argv )
{

    int width = 1024, height= 768;
    float widthf = (float) width, heightf = (float) height;
    double t;
    bool blackFade = false;
    //float fps = 0.f;
    float gamma = 1.0;
    int blurSampleCount = 5;
    glm::vec3 focus(7,7.857,8.25);
    glm::vec3 spotPos(0.0,10.0,0.0);


    int shadowSampleCount = 16;
    float shadowSpread = 1500.0;

    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GL_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);


#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    int const DPI = 2; // For retina screens only
#else
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    int const DPI = 1;
# endif

    // Open a window and create its OpenGL context
    GLFWwindow * window = glfwCreateWindow(width/DPI, height/DPI, "Démo ! :D", 0, 0);
    if( ! window )
    {
        fprintf( stderr, "Failed to open GLFW window\n" );
        glfwTerminate();
        exit( EXIT_FAILURE );
    }
    glfwMakeContextCurrent(window);

    // Init glew
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
          /* Problem: glewInit failed, something is seriously wrong. */
          fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
          exit( EXIT_FAILURE );
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode( window, GLFW_STICKY_KEYS, GL_TRUE );

    // Enable vertical sync (on cards that support it)
    glfwSwapInterval( 1 );
    GLenum glerr = GL_NO_ERROR;
    glerr = glGetError();

    ImGui_ImplGlfwGL3_Init(window, true);

    // Init viewer structures
    Camera camera;
    camera_defaults(camera);
    GUIStates guiStates;
    init_gui_states(guiStates);
    int instanceCount = 0;
    int spotLightCount = 20;
    float speed = 1.0;
    float scaleFactor=0.01f;


    // Load images and upload textures
    GLuint textures[4];
    glGenTextures(4, textures);
    int x;
    int y;
    int comp;

    //grass
    unsigned char * diffuse = stbi_load("textures/grass.png", &x, &y, &comp, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    fprintf(stderr, "Diffuse %dx%d:%d\n", x, y, comp);

    unsigned char * spec = stbi_load("textures/grass.png", &x, &y, &comp, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, x, y, 0, GL_RED, GL_UNSIGNED_BYTE, spec);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    fprintf(stderr, "Spec %dx%d:%d\n", x, y, comp);

    //water
    unsigned char * diffuseWater = stbi_load("textures/blue.jpg", &x, &y, &comp, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuseWater);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    fprintf(stderr, "diffuseWater %dx%d:%d\n", x, y, comp);

    unsigned char * specWater = stbi_load("textures/blue.jpg", &x, &y, &comp, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[3]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, x, y, 0, GL_RED, GL_UNSIGNED_BYTE, specWater);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    fprintf(stderr, "specWater %dx%d:%d\n", x, y, comp);
    checkError("Texture Initialization");


    // Try to load and compile blit shaders
    GLuint vertBlitShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "blit.vert");
    GLuint fragBlitShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "blit.frag");
    GLuint blitProgramObject = glCreateProgram();
    glAttachShader(blitProgramObject, vertBlitShaderId);
    glAttachShader(blitProgramObject, fragBlitShaderId);
    glLinkProgram(blitProgramObject);
    if (check_link_error(blitProgramObject) < 0)
        exit(1);

    // Try to load and compile gbuffer shaders
    GLuint vertgbufferShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "gbuffer.vert");
    GLuint fraggbufferShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "gbuffer.frag");
    GLuint gbufferProgramObject = glCreateProgram();
    glAttachShader(gbufferProgramObject, vertgbufferShaderId);
    glAttachShader(gbufferProgramObject, fraggbufferShaderId);
    glLinkProgram(gbufferProgramObject);
    if (check_link_error(gbufferProgramObject) < 0)
        exit(1);

    // Try to load and compile shadow shaders
    GLuint fragshadowShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "shadow.frag");
    GLuint shadowProgramObject = glCreateProgram();
    glAttachShader(shadowProgramObject, vertgbufferShaderId);
    glAttachShader(shadowProgramObject, fragshadowShaderId);
    glLinkProgram(shadowProgramObject);
    if (check_link_error(shadowProgramObject) < 0)
        exit(1);

    // Try to load and compile spotlight shaders
    GLuint fragspotlightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "spotlight.frag");
    GLuint spotlightProgramObject = glCreateProgram();
    glAttachShader(spotlightProgramObject, vertBlitShaderId);
    glAttachShader(spotlightProgramObject, fragspotlightShaderId);
    glLinkProgram(spotlightProgramObject);
    if (check_link_error(spotlightProgramObject) < 0)
        exit(1);

    // Try to load and compile gamma shaders
    GLuint fragGammaId = compile_shader_from_file(GL_FRAGMENT_SHADER, "gamma.frag");
    GLuint gammaProgramObject = glCreateProgram();
    glAttachShader(gammaProgramObject, vertBlitShaderId);
    glAttachShader(gammaProgramObject, fragGammaId);
    glLinkProgram(gammaProgramObject);
    if (check_link_error(gammaProgramObject) < 0)
        exit(1);

    // Try to load and compile blur shaders
    GLuint fragBlurId = compile_shader_from_file(GL_FRAGMENT_SHADER, "blur.frag");
    GLuint blurProgramObject = glCreateProgram();
    glAttachShader(blurProgramObject, vertBlitShaderId);
    glAttachShader(blurProgramObject, fragBlurId);
    glLinkProgram(blurProgramObject);
    if (check_link_error(blurProgramObject) < 0)
        exit(1);

    // Try to load and compile COC shaders
    GLuint fragCOCId = compile_shader_from_file(GL_FRAGMENT_SHADER, "COC.frag");
    GLuint COCProgramObject = glCreateProgram();
    glAttachShader(COCProgramObject, vertBlitShaderId);
    glAttachShader(COCProgramObject, fragCOCId);
    glLinkProgram(COCProgramObject);
    if (check_link_error(COCProgramObject) < 0)
        exit(1);

    // Try to load and compile DoF shaders
    GLuint fragDoFId = compile_shader_from_file(GL_FRAGMENT_SHADER, "DepthOfField.frag");
    GLuint DoFProgramObject = glCreateProgram();
    glAttachShader(DoFProgramObject, vertBlitShaderId);
    glAttachShader(DoFProgramObject, fragDoFId);
    glLinkProgram(DoFProgramObject);
    if (check_link_error(DoFProgramObject) < 0)
        exit(1);

    // Try to load and compile our fade to black shader
    GLuint fragFadeId = compile_shader_from_file(GL_FRAGMENT_SHADER, "Fade.frag");
    GLuint FadeProgramObject = glCreateProgram();
    glAttachShader(FadeProgramObject, vertBlitShaderId);
    glAttachShader(FadeProgramObject, fragFadeId);
    glLinkProgram(FadeProgramObject);
    if (check_link_error(FadeProgramObject) < 0)
        exit(1);

    // Upload uniforms

    GLuint mvpLocation = glGetUniformLocation(gbufferProgramObject, "MVP");
    GLuint mvLocation = glGetUniformLocation(gbufferProgramObject, "MV");
    GLuint timeLocation = glGetUniformLocation(gbufferProgramObject, "Time");
    GLuint diffuseLocation = glGetUniformLocation(gbufferProgramObject, "Diffuse");
    GLuint diffuseColorLocation = glGetUniformLocation(gbufferProgramObject, "DiffuseColor");
    GLuint specLocation = glGetUniformLocation(gbufferProgramObject, "Specular");
    GLuint specularPowerLocation = glGetUniformLocation(gbufferProgramObject, "SpecularPower");
    GLuint instanceCountLocation = glGetUniformLocation(gbufferProgramObject, "InstanceCount");
    GLuint blitTextureLocation = glGetUniformLocation(blitProgramObject, "Texture");
    glProgramUniform1i(blitProgramObject, blitTextureLocation, 0);

    GLuint spotlightColorLocation = glGetUniformLocation(spotlightProgramObject, "ColorBuffer");
    GLuint spotlightNormalLocation = glGetUniformLocation(spotlightProgramObject, "NormalBuffer");
    GLuint spotlightDepthLocation = glGetUniformLocation(spotlightProgramObject, "DepthBuffer");
    GLuint spotlightShadowLocation = glGetUniformLocation(spotlightProgramObject, "Shadow");
    GLuint spotlightLightLocation = glGetUniformBlockIndex(spotlightProgramObject, "light");
    GLuint spotInverseProjectionLocation = glGetUniformLocation(spotlightProgramObject, "InverseProjection");
    GLuint spotShadowSampleCountLocation = glGetUniformLocation(spotlightProgramObject, "SampleCount");
    GLuint spotShadowSpreadLocation = glGetUniformLocation(spotlightProgramObject, "Spread");

    glProgramUniform1i(spotlightProgramObject, spotlightColorLocation, 0);
    glProgramUniform1i(spotlightProgramObject, spotlightNormalLocation, 1);
    glProgramUniform1i(spotlightProgramObject, spotlightDepthLocation, 2);
    glProgramUniform1i(spotlightProgramObject, spotlightShadowLocation, 3);

    GLuint shadowMVPLocation = glGetUniformLocation(shadowProgramObject, "MVP");
    GLuint shadowMVLocation = glGetUniformLocation(shadowProgramObject, "MV");
    GLuint shadowTimeLocation = glGetUniformLocation(shadowProgramObject, "Time");
    GLuint shadowInstanceCountLocation = glGetUniformLocation(shadowProgramObject, "InstanceCount");

    //gamma uniform
    GLuint gammaLocation = glGetUniformLocation(gammaProgramObject, "Gamma");
    GLuint gammaTextureLocation = glGetUniformLocation(gammaProgramObject, "Texture");
    glProgramUniform1i(gammaProgramObject, gammaTextureLocation, 0);

    //blur
    GLuint blurSampleCountLocation = glGetUniformLocation(blurProgramObject, "SampleCount");
    GLuint blurDirectionLocation = glGetUniformLocation(blurProgramObject, "Direction");
    GLuint blurTextureLocation = glGetUniformLocation(blurProgramObject, "Texture");
    glProgramUniform1i(blurProgramObject, blurTextureLocation, 0);

    //COC
    GLuint COCScreenToViewLocation = glGetUniformLocation(COCProgramObject, "ScreenToView");
    GLuint COCFocusLocation = glGetUniformLocation(COCProgramObject, "Focus");
    GLuint COCTextureLocation = glGetUniformLocation(COCProgramObject, "Texture");
    glProgramUniform1i(COCProgramObject, COCTextureLocation, 0);

    //DoF
    GLuint DoFColorLocation = glGetUniformLocation(DoFProgramObject, "Color");
    glProgramUniform1i(DoFProgramObject, DoFColorLocation, 0);
    GLuint DoFBlurLocation = glGetUniformLocation(DoFProgramObject, "Blur");
    glProgramUniform1i(DoFProgramObject, DoFBlurLocation, 2);
    GLuint DoFCoCLocation = glGetUniformLocation(DoFProgramObject, "CoC");
    glProgramUniform1i(DoFProgramObject, DoFCoCLocation, 1);

    //Fade
    GLuint FadeTimeLocation = glGetUniformLocation(FadeProgramObject, "t");
    GLuint FadeStartTimeLocation = glGetUniformLocation(FadeProgramObject, "startTime");
    GLuint FadeColorLocation = glGetUniformLocation(FadeProgramObject, "Color");
    glProgramUniform1i(FadeProgramObject, FadeColorLocation, 0);

   if (!checkError("Uniforms"))
        exit(1);



/*------------ ASSIMP ---------------*/

    const aiScene* scene = NULL;
    GLuint scene_list = 0;
    char* objPath = "../flower/StrangeFlower.obj";

    scene = aiImportFile(objPath, aiProcessPreset_TargetRealtime_MaxQuality);

    if (!scene) {
          fprintf(stderr, "Error: impossible to open %s\n", objPath);
          exit( EXIT_FAILURE );
    }



    GLuint * assimp_vao = new GLuint[scene->mNumMeshes];
    glm::mat4 * assimp_objectToWorld = new glm::mat4[scene->mNumMeshes];
    glGenVertexArrays(scene->mNumMeshes, assimp_vao);
    GLuint * assimp_vbo = new GLuint[scene->mNumMeshes*4];
    glGenBuffers(scene->mNumMeshes*4, assimp_vbo);

    float * assimp_diffuse_colors = new float[scene->mNumMeshes*3];
    GLuint * assimp_diffuse_texture_ids = new GLuint[scene->mNumMeshes*3];

    for (GLuint i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* m = scene->mMeshes[i];
        GLuint * faces = new GLuint[m->mNumFaces*3];
        for (GLuint j = 0; j < m->mNumFaces; ++j)
        {
            const aiFace& f = m->mFaces[j];
            faces[j*3] = f.mIndices[0];
            faces[j*3+1] = f.mIndices[1];
            faces[j*3+2] = f.mIndices[2];
        }
      
        glBindVertexArray(assimp_vao[i]);
        // Bind indices and upload data
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, assimp_vbo[i*4]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->mNumFaces*3 *sizeof(GLuint), faces, GL_STATIC_DRAW);

        // Bind vertices and upload data
        glBindBuffer(GL_ARRAY_BUFFER, assimp_vbo[i*4+1]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mVertices, GL_STATIC_DRAW);
        
        // Bind normals and upload data
        glBindBuffer(GL_ARRAY_BUFFER, assimp_vbo[i*4+2]);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mNormals, GL_STATIC_DRAW);
        
        // Bind uv coords and upload data
        glBindBuffer(GL_ARRAY_BUFFER, assimp_vbo[i*4+3]);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        if (m->HasTextureCoords(0))
            glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mTextureCoords[0], GL_STATIC_DRAW);
        else
            glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mVertices, GL_STATIC_DRAW);
        delete[] faces;

        const aiMaterial * mat = scene->mMaterials[m->mMaterialIndex];
        int texIndex = 0;
        aiString texPath;
        if(AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, texIndex, &texPath))
        {
            std::string path = objPath;
            size_t pos = path.find_last_of("\\/");
            std::string basePath = (std::string::npos == pos) ? "" : path.substr(0, pos + 1);
            std::string fileloc = basePath + texPath.data;
            pos = fileloc.find_last_of("\\");
            fileloc = (std::string::npos == pos) ? fileloc : fileloc.replace(pos, 1, "/");
            int x;
            int y;
            int comp;
            glGenTextures(1, assimp_diffuse_texture_ids + i);
            unsigned char * diffuse = stbi_load(fileloc.c_str(), &x, &y, &comp, 3);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, assimp_diffuse_texture_ids[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuse);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            assimp_diffuse_texture_ids[i] = 0;           
        }
        aiColor4D diffuse;
        if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
        {
            assimp_diffuse_colors[i*3] = diffuse.r;
            assimp_diffuse_colors[i*3+1] = diffuse.g;
            assimp_diffuse_colors[i*3+2] = diffuse.b;
        }
        else
        {
            assimp_diffuse_colors[i*3] = 1.f;
            assimp_diffuse_colors[i*3+1] = 0.f;
            assimp_diffuse_colors[i*3+2] = 1.f;
        }
    }

    std::stack<aiNode*> stack;
    stack.push(scene->mRootNode);
    while(stack.size()>0)
    {
        aiNode * node = stack.top();
        for (GLuint i =0; i < node->mNumMeshes; ++i)
        {
            aiMatrix4x4 t = node->mTransformation;
            assimp_objectToWorld[i] = glm::mat4(t.a1, t.a2, t.a3, t.a4,
                                                t.b1, t.b2, t.b3, t.b4,
                                                t.c1, t.c2, t.c3, t.c4,
                                                t.d1, t.d2, t.d3, t.d4);
        }
        stack.pop();
    }

    // Unbind everything. Potentially illegal on some implementations
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

/******** END ASSIMP ************/

    // Load geometry
    int cube_triangleCount = 12;
    int cube_triangleList[] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 19, 17, 20, 21, 22, 23, 24, 25, 26, };
    float cube_uvs[] = {0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f,  1.f, 1.f,  0.f, 0.f, 0.f, 0.f, 1.f, 1.f,  1.f, 0.f,  };
    float cube_vertices[] = {-0.5, -0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5 };
    float cube_normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, };
    int plane_triangleCount = 2;
    int plane_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float plane_uvs[] = {0.f, 0.f, 0.f, 50.f, 50.f, 0.f, 50.f, 50.f};
    float plane_vertices[] = {-50.0, -1.0, 50.0, 50.0, -1.0, 50.0, -50.0, -1.0, -50.0, 50.0, -1.0, -50.0};
    float plane_normals[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    int   quad_triangleCount = 2;
    int   quad_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float quad_vertices[] =  {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    // Vertex Array Object
    GLuint vao[3];
    glGenVertexArrays(3, vao);

    // Vertex Buffer Objects
    GLuint vbo[10];
    glGenBuffers(10, vbo);

    // Cube
    glBindVertexArray(vao[0]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_triangleList), cube_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_normals), cube_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[3]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_uvs), cube_uvs, GL_STATIC_DRAW);

    // Plane
    glBindVertexArray(vao[1]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[4]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_triangleList), plane_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[5]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[6]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_normals), plane_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[7]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_uvs), plane_uvs, GL_STATIC_DRAW);

    // Quad
    glBindVertexArray(vao[2]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[8]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_triangleList), quad_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[9]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Unbind everything. Potentially illegal on some implementations
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Update and bind uniform buffer object
    const int SPOT_LIGHT_COUNT = 20;
    GLuint ubo[1];
    glGenBuffers(1, ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
    GLint uboOffset = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uboOffset);
    GLint uboSize = 0;
    glGetActiveUniformBlockiv(spotlightProgramObject, spotlightLightLocation, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
    uboSize = ((uboSize / uboOffset) + 1) * uboOffset;
    glBufferData(GL_UNIFORM_BUFFER, uboSize * SPOT_LIGHT_COUNT, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Init frame buffers
    GLuint gbufferFbo;
    GLuint gbufferTextures[3];
    GLuint gbufferDrawBuffers[2];
    glGenTextures(3, gbufferTextures);

    // Create color texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create normal texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create Framebuffer Object
    glGenFramebuffers(1, &gbufferFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);
    gbufferDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    gbufferDrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(2, gbufferDrawBuffers);

    // Attach textures to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, gbufferTextures[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 , GL_TEXTURE_2D, gbufferTextures[1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gbufferTextures[2], 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building gbuffer framebuffer\n");
        exit( EXIT_FAILURE );
    }

    // Create shadow FBO
    GLuint shadowFbo;
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    
    // Create shadow textures
    const int SPOT_LIGHT_SHADOW_RES = 2048;
    GLuint shadowTextures[SPOT_LIGHT_COUNT];
    glGenTextures(SPOT_LIGHT_COUNT, shadowTextures);
    for (int i = 0; i < SPOT_LIGHT_COUNT; ++i) 
    {
        glBindTexture(GL_TEXTURE_2D, shadowTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SPOT_LIGHT_SHADOW_RES, SPOT_LIGHT_SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
#if 1        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,  GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC,  GL_LEQUAL);
#endif        
    }

    // Create a render buffer since we don't need to read shadow color 
    // in a texture
    GLuint shadowRenderBuffer;
    glGenRenderbuffers(1, &shadowRenderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, shadowRenderBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, SPOT_LIGHT_SHADOW_RES, SPOT_LIGHT_SHADOW_RES);

    // Attach the first texture to the depth attachment
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTextures[0], 0);

    // Attach the renderbuffer
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, shadowRenderBuffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building shadow framebuffer\n");
        exit( EXIT_FAILURE );
    }


    //--------------------------------------------------- Create Fx Framebuffer Object


    GLuint fxFbo;
    GLuint fxDrawBuffers[1];
    glGenFramebuffers(1, &fxFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fxFbo);
    fxDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, fxDrawBuffers);

            // Create Fx textures
    const int FX_TEXTURE_COUNT = 4;
    GLuint fxTextures[FX_TEXTURE_COUNT];
    glGenTextures(FX_TEXTURE_COUNT, fxTextures);
    for (int i = 0; i < FX_TEXTURE_COUNT; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, fxTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Attach first fx texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[0], 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building framebuffern");
        exit( EXIT_FAILURE );
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    do
    {
        t = glfwGetTime() * speed;
        ImGui_ImplGlfwGL3_NewFrame();

        // Mouse states
        int leftButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT );
        int rightButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_RIGHT );
        int middleButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_MIDDLE );

        if( leftButton == GLFW_PRESS )
            guiStates.turnLock = true;
        else
            guiStates.turnLock = false;

        if( rightButton == GLFW_PRESS )
            guiStates.zoomLock = true;
        else
            guiStates.zoomLock = false;

        if( middleButton == GLFW_PRESS )
            guiStates.panLock = true;
        else
            guiStates.panLock = false;

        // Camera movements
        int altPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
        if (!altPressed && (leftButton == GLFW_PRESS || rightButton == GLFW_PRESS || middleButton == GLFW_PRESS))
        {
            double x; double y;
            glfwGetCursorPos(window, &x, &y);
            guiStates.lockPositionX = x;
            guiStates.lockPositionY = y;
        }
        if (altPressed == GLFW_PRESS)
        {
            double mousex; double mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            int diffLockPositionX = mousex - guiStates.lockPositionX;
            int diffLockPositionY = mousey - guiStates.lockPositionY;
            if (guiStates.zoomLock)
            {
                float zoomDir = 0.0;
                if (diffLockPositionX > 0)
                    zoomDir = -1.f;
                else if (diffLockPositionX < 0 )
                    zoomDir = 1.f;
                camera_zoom(camera, zoomDir * GUIStates::MOUSE_ZOOM_SPEED);
            }
            else if (guiStates.turnLock)
            {
                camera_turn(camera, diffLockPositionY * GUIStates::MOUSE_TURN_SPEED,
                            diffLockPositionX * GUIStates::MOUSE_TURN_SPEED);

            }
            else if (guiStates.panLock)
            {
                camera_pan(camera, diffLockPositionX * GUIStates::MOUSE_PAN_SPEED,
                            diffLockPositionY * GUIStates::MOUSE_PAN_SPEED);
            }
            guiStates.lockPositionX = mousex;
            guiStates.lockPositionY = mousey;
        }
        if (glfwGetKey(window, GLFW_KEY_G))
            guiStates.display = ! guiStates.display;
        if (glfwGetKey(window, GLFW_KEY_B))
            guiStates.blit = ! guiStates.blit;
        if (!blackFade && glfwGetKey(window, GLFW_KEY_SPACE)){
            blackFade = true;
            glProgramUniform1f(FadeProgramObject, FadeStartTimeLocation, t);
        }

        // Default states
        glEnable(GL_DEPTH_TEST);

        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Viewport 
        glViewport( 0, 0, width, height  );

        // Bind gbuffer
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);

        // Clear the gbuffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Get camera matrices
        glm::mat4 projection = glm::perspective(45.0f, widthf / heightf, 0.1f, 100.f); 
        glm::mat4 worldToView = glm::lookAt(camera.eye, camera.o, camera.up);
        glm::mat4 objectToWorld;
        glm::mat4 mv = worldToView * objectToWorld;
        glm::mat4 mvp = projection * mv;
        //glm::mat4 inverseProjection = glm::transpose(glm::inverse(projection));
        glm::mat4 inverseProjection = glm::inverse(projection);

        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);



        // Upload uniforms

        glProgramUniformMatrix4fv(gbufferProgramObject, mvpLocation, 1, 0, glm::value_ptr(mvp));
        glProgramUniformMatrix4fv(gbufferProgramObject, mvLocation, 1, 0, glm::value_ptr(mv));
        glProgramUniform1i(gbufferProgramObject, instanceCountLocation, (int) instanceCount);
        glProgramUniform1f(gbufferProgramObject, specularPowerLocation, 5.f);
        glProgramUniform1f(gbufferProgramObject, timeLocation, t);
        glProgramUniform1f(FadeProgramObject, FadeTimeLocation, t);
        glProgramUniform1i(gbufferProgramObject, diffuseLocation, 0);
        glProgramUniform1i(gbufferProgramObject, specLocation, 1);
        glProgramUniformMatrix4fv(spotlightProgramObject, spotInverseProjectionLocation, 1, 0, glm::value_ptr(inverseProjection));
        glProgramUniform1i(shadowProgramObject, shadowInstanceCountLocation, (int) instanceCount);
        glProgramUniform1f(gammaProgramObject, gammaLocation, gamma);
        glProgramUniformMatrix4fv(COCProgramObject,COCScreenToViewLocation, 1, 0, glm::value_ptr(inverseProjection));
        glProgramUniform3f(COCProgramObject,COCFocusLocation,focus.x,focus.y,focus.z);
        glProgramUniform1i(spotlightProgramObject, spotShadowSampleCountLocation, shadowSampleCount);
        glProgramUniform1f(spotlightProgramObject, spotShadowSpreadLocation, shadowSpread);


        // Select shader
        glUseProgram(gbufferProgramObject);
        glActiveTexture(GL_TEXTURE0);
        GLuint subIndex = 0;
        glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &subIndex);

        // Render vaos
        glBindVertexArray(vao[0]);
        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[3]);
        glDrawElementsInstanced(GL_TRIANGLES, cube_triangleCount * 3, GL_UNSIGNED_INT, (void*)0, (int) instanceCount);
        //glDrawElements(GL_TRIANGLES, cube_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        glProgramUniform1f(gbufferProgramObject, timeLocation, 0.f);
        glBindVertexArray(vao[1]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glDrawElements(GL_TRIANGLES, plane_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);


        //glUseProgram(assimpProgramObject);
        glActiveTexture(GL_TEXTURE0);
        // Render assimp vaos
        glm::mat4 scale = glm::scale(glm::mat4(), glm::vec3(scaleFactor));


        float translation = t - 3.5f;
        translation = glm::min(translation,2.f);
        
        glm::mat4 translate = glm::translate(glm::mat4(), glm::vec3(-1,-3.5,0.4));
        glm::mat4 rotate = glm::rotate(glm::mat4(),0.0f, glm::vec3(0,1,0));

        float flowerRisingTime  = 15.f;
        float flowerUpTime  = 30.f;
        float waterStart = 5.f;
        float waterEnd = 10.f;

        if(t>waterStart && t<waterEnd){
            instanceCount = (t-waterStart) * 125;
        }else if(t > waterStart && t < flowerUpTime) {
            instanceCount = glm::max(1000 - ((t-waterStart) * 50), 0.0);
        }
        if(t > flowerRisingTime && t < flowerUpTime){

            translate = glm::translate(glm::mat4(), glm::vec3(-1.0,(5.5 / 15.0) * t -9, 0.4));
            rotate = glm::rotate(glm::mat4(),(float)t, glm::vec3(0,1,0));
        }
        if(t > flowerUpTime){
            translate = glm::translate(glm::mat4(), glm::vec3(-1.0,2, 0.4));
            rotate = glm::rotate(glm::mat4(), 30.f , glm::vec3(0,1,0));
        }

        for (GLuint i =0; i < scene->mNumMeshes; ++i)
        {
            subIndex = 1;
            if (assimp_diffuse_texture_ids[i] > 0) {
                glBindTexture(GL_TEXTURE_2D, assimp_diffuse_texture_ids[i]);
                subIndex = 0;
            }
            glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &subIndex);
            
            mv = worldToView * rotate * translate * scale * assimp_objectToWorld[i];
            mvp = projection * mv;
            #if 0
            #endif
            glProgramUniformMatrix4fv(gbufferProgramObject, mvpLocation, 1, 0, glm::value_ptr(mvp));
            glProgramUniformMatrix4fv(gbufferProgramObject, mvLocation, 1, 0, glm::value_ptr(mv));
            glProgramUniform3fv(gbufferProgramObject, diffuseColorLocation, 1, assimp_diffuse_colors + 3*i);
            const aiMesh* m = scene->mMeshes[i];
            glBindVertexArray(assimp_vao[i]);
            glDrawElements(GL_TRIANGLES, m->mNumFaces * 3, GL_UNSIGNED_INT, (void*)0);
            
        }




        // Shadow passes
        // Map the spot light data UBO
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
        char * spotLightBuffer = (char *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize * SPOT_LIGHT_COUNT, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        // Bind the shadow FBO
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        // Use shadow program
        glUseProgram(shadowProgramObject);
        glViewport(0, 0, SPOT_LIGHT_SHADOW_RES, SPOT_LIGHT_SHADOW_RES);
        for (int i = 0; i < spotLightCount; ++i)
        {
            // Setup light data
#if 0                 
            //Spotlights tournant et pointant le centre

            float iF = 40.f;
            glm::vec3 lp(8*cosf(2*M_PI/spotLightCount*i + t), 7.f, 8*sinf(2*M_PI/spotLightCount*i + t));
            glm::vec3 ld(-cosf(2*M_PI/spotLightCount*i + t), -1.0, -sinf(2*M_PI/spotLightCount*i + t));
            float angle = 40.f;
            float penumbraAngle = angle + 10.f;
            glm::vec3 color(1.0, 1.0, 1.0);

#else
            float iF;
            glm::vec3 lp;
            glm::vec3 ld;
            float angle;
            float penumbraAngle;
            glm::vec3 color;

            if(i < 3){
                iF = 12.f;
                lp = glm::vec3(8*cosf(2*M_PI/3*i), 7.f, 8*sinf(2*M_PI/3*i));
                ld = -lp;
                angle = 70.f;
                penumbraAngle = angle + 10.f;
                color = glm::vec3(1.0, 1.0, 1.0);
            } else if(i < 9){
                int idx = i-3;
                iF = glm::max(glm::min(25.0,t*20-6*i),0.0);
                lp = glm::vec3((idx - 3)*2, 3.f, cos(idx*idx)*2);
                ld = glm::vec3(sinf(50*i)*0.2,-1.f,sinf(10*i)*0.15);
                angle = 30.f + cos(i*i)*10.f;
                penumbraAngle = angle + 10.f;
                color = glm::vec3(0.1 + cosf(50*idx)*0.25, 0.3 + cosf(idx*idx)*0.2, 0.7 + cosf(idx)*0.3);
            } else if (i == 9){
                iF = 0;
                angle = 0.f;

                if(t < flowerRisingTime || t > flowerUpTime +3){
                    iF = 0;
                    angle = 0;
                }
                else if(t < flowerRisingTime +2){
                    iF = (t - flowerRisingTime) * 15.f / 2.f;
                    angle = (t - flowerRisingTime) * 30.f / 2.f;
                }else if (t < flowerUpTime -3){
                    iF = 15.f;
                    angle = 30;
                }else{
                    iF = (flowerUpTime +3 - t)* 15.f /6.f;
                    angle = (flowerUpTime +3 - t)* 30.f /6.f;
                }


                lp = glm::vec3(0,6.f,0);
                ld = glm::vec3(0,-1.f,0);
                //angle = 30.f;
                penumbraAngle = angle*1.1f;
                color = glm::vec3(1,1,1);
            }else if (i < spotLightCount){
                int idx = i - 10;
                int nbStar = spotLightCount-10;

                iF = 0.f;
                if(t < waterStart-2){
                    iF = 0;
                    angle = 0;
                }
                else if(t < waterStart){
                    iF = (t - waterStart+2) * 75.f/2.f;
                    angle = (t - waterStart+2) * 10.f/2.f;
                }else{
                    iF = 75.f;
                    angle = 10;
                }

                lp = glm::vec3(6*cosf(2*M_PI/nbStar*idx + t), 15.f, 6*sinf(2*M_PI/nbStar*idx + t));
                ld = glm::vec3(cosf(2*M_PI/nbStar*idx + t)*0.2*cosf(t*(3.5 + (float)idx / (nbStar * 2)) +idx*i),
                         -1.0, sinf(2*M_PI/nbStar*idx + t)*0.2*cosf(t*(3.5 + (float)idx / (nbStar * 2)) +idx*i));
                //angle = 40.f;
                penumbraAngle = angle * 1.2;
                color = glm::vec3(cosf(i*idx), cosf(i*i), cosf(idx*idx)); 
            }


#endif            
            // Light space matrices
            glm::mat4 projection = glm::perspective(glm::radians(penumbraAngle*2.f), 1.f, 1.f, 100.f); 
            glm::mat4 worldToLight = glm::lookAt(lp, lp + ld, glm::vec3(0.f, 0.f, -1.f));
            glm::mat4 objectToWorld;
            glm::mat4 objectToLight = worldToLight * objectToWorld;
            glm::mat4 objectToLightScreen = projection * objectToLight;
            SpotLight s = { 
                glm::vec3( worldToView * glm::vec4(lp, 1.f)), angle,
                glm::vec3( worldToView * glm::vec4(ld, 0.f)), penumbraAngle,
                color, iF, 
                projection * worldToLight * glm::inverse(worldToView)
            };
            *((SpotLight *) (spotLightBuffer + i * uboSize)) = s;
            // Attach shadow texture for current light
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTextures[i], 0);
            // Clear only the depth buffer
            glClear(GL_DEPTH_BUFFER_BIT);


            // Update scene uniforms
            glProgramUniformMatrix4fv(shadowProgramObject, shadowMVPLocation, 1, 0, glm::value_ptr(objectToLightScreen));
            glProgramUniformMatrix4fv(shadowProgramObject, shadowMVLocation, 1, 0, glm::value_ptr(objectToLight));


            // Render assimp vaos
            glActiveTexture(GL_TEXTURE0);
            glm::mat4 scale = glm::scale(glm::mat4(), glm::vec3(scaleFactor)); 
            for (GLuint i =0; i < scene->mNumMeshes; ++i)
            {
                mv = worldToLight * rotate * translate * scale * assimp_objectToWorld[i];
                mvp = projection * mv;
                
                glProgramUniformMatrix4fv(shadowProgramObject, shadowMVPLocation, 1, 0, glm::value_ptr(mvp));
                glProgramUniformMatrix4fv(shadowProgramObject, shadowMVLocation, 1, 0, glm::value_ptr(mv));
                const aiMesh* m = scene->mMeshes[i];
                glBindVertexArray(assimp_vao[i]);

                glDrawElements(GL_TRIANGLES, m->mNumFaces * 3, GL_UNSIGNED_INT, (void*)0);
                
                
            }
            // Render the scene
            glProgramUniform1f(shadowProgramObject, shadowTimeLocation, t);
            glBindVertexArray(vao[0]);
            glDrawElementsInstanced(GL_TRIANGLES, cube_triangleCount * 3, GL_UNSIGNED_INT, (void*)0, (int) instanceCount);
            glProgramUniform1f(shadowProgramObject, shadowTimeLocation, 0.f);
            glBindVertexArray(vao[1]);
            glDrawElements(GL_TRIANGLES, plane_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);


        }        
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
        glActiveTexture(GL_TEXTURE3);

        // Bind the same VAO for all lights
        glBindVertexArray(vao[2]);

        //bind FX framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fxFbo);


        //select texture to write in
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fxTextures[0], 0);
        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // Render spot lights
        glUseProgram(spotlightProgramObject);
        for (int i = 0; i < spotLightCount; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, shadowTextures[i]);
            glBindBufferRange(GL_UNIFORM_BUFFER, spotlightLightLocation, ubo[0], uboSize * i, uboSize);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        /* -------------- BASIC BLUR -------------- */

        if(blurSampleCount){
            // Use blur program shader
            glUseProgram(blurProgramObject);

            //upload uniforms
            glProgramUniform1i(blurProgramObject, blurSampleCountLocation, blurSampleCount);
            glProgramUniform2i(blurProgramObject, blurDirectionLocation, 0, 1);


            // Write into Vertical Blur Texture
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[1], 0);
            // Clear the content of texture
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,fxTextures[0] );
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

            //upload uniform
            glProgramUniform2i(blurProgramObject, blurDirectionLocation, 1, 0);
            // Write into Horizontal Blur Texture
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[2] , 0);
            // Clear the content of texture
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            // Read the texture processed by the Vertical Blur
            glBindTexture(GL_TEXTURE_2D, fxTextures[1]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

            /* -------------- COC -------------- */
            // Use circle of confusion program shader
            glUseProgram(COCProgramObject);

            // Write into Circle of Confusion Texture
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[1], 0);
            // Clear the content of  texture
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            // Read the depth texture
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]); //PAS TOUCHE
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

            /* -------------- DEPTH OF FIELD -------------- */
            // Use the Depth of Field shader
            glUseProgram(DoFProgramObject);

            // Attach Depth of Field texture to framebuffer
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[3], 0);
            // Only the color buffer is used
            glClear(GL_COLOR_BUFFER_BIT);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fxTextures[0]); // Color
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, fxTextures[1]); // CoC
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, fxTextures[2]); // Blur
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        /* -------------- GAMMA -------------- */
        glUseProgram(gammaProgramObject);

        glActiveTexture(GL_TEXTURE0);

        //select the texture to read from
        if(blurSampleCount)
            glBindTexture(GL_TEXTURE_2D, fxTextures[3]);
        else
            glBindTexture(GL_TEXTURE_2D, fxTextures[0]);
        //select the texture to write in
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fxTextures[1], 0);
        //clear the buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //draw the gamma-treated image
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        if(blackFade){
            glUseProgram(FadeProgramObject);
            glBindTexture(GL_TEXTURE_2D, fxTextures[1]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fxTextures[3], 0);
            //clear the buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            //draw the faded image
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }
        
        // Default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Full screen viewport
        glViewport(0, 0, width, height);
        // Clear default framebuffer color buffer
        glClear(GL_COLOR_BUFFER_BIT);
        // Disable depth test
        glDisable(GL_DEPTH_TEST);

        // Display
        glUseProgram(blitProgramObject);
        glActiveTexture(GL_TEXTURE0);
        if(blackFade)
            glBindTexture(GL_TEXTURE_2D, fxTextures[3]);
        else
            glBindTexture(GL_TEXTURE_2D, fxTextures[1]);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        glDisable(GL_BLEND);

        if (guiStates.blit)
        {
            // Bind blit shader
            glUseProgram(blitProgramObject);
            // Upload uniforms
            // use only unit 0
            glActiveTexture(GL_TEXTURE0);
            // Bind VAO
            glBindVertexArray(vao[2]);

            // Viewport 
            glViewport( 0, 0, width/4, height/4  );
            // Bind texture
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
            // Draw quad
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
            // Viewport 
            glViewport( width/4, 0, width/4, height/4  );
            // Bind texture
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
            // Draw quad
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
            // Viewport 
            glViewport( width/4 * 2, 0, width/4, height/4  );
            // Bind texture
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
            // Draw quad
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
            // Viewport 
            glViewport( width/4 * 3, 0, width/4, height/4 );
            // Bind texture
            glBindTexture(GL_TEXTURE_2D, fxTextures[1]);
            // Draw quad
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        if (guiStates.display)
        {

            ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
            ImGui::Begin("aogl");
            ImGui::SliderFloat("Gamma", &gamma, 0.1, 6.0);
            ImGui::SliderInt("blur Sample count", &blurSampleCount, 0, 20);
            ImGui::SliderFloat("Focus near", &focus.x, 0.0, 15.0);
            ImGui::SliderFloat("Focus center", &focus.y, 0.0, 15.0);
            ImGui::SliderFloat("Focus far", &focus.z, 0.0, 15.0);
            ImGui::SliderInt("shadow Sample Count", &shadowSampleCount, 0, 16);
            ImGui::SliderFloat("Shadow spread", &shadowSpread, 0.0, 3000.0);

            float* data = glm::value_ptr(spotPos);
            ImGui::SliderFloat3("light pos", data, -2.0f, 2.0f);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            ImGui::Text("Phi %.1f; theta %.1f", camera.phi, camera.theta);


            ImGui::End();
            ImGui::Render();
        }

        // Check for errors
        checkError("End loop");

        glfwSwapBuffers(window);
        glfwPollEvents();

        //double newTime = glfwGetTime();
        //fps = 1.f/ (newTime - t);
    } // Check if the ESC key was pressed
    while( glfwGetKey( window, GLFW_KEY_ESCAPE ) != GLFW_PRESS );

    // Close OpenGL window and terminate GLFW
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    exit( EXIT_SUCCESS );
}

// No windows implementation of strsep
char * strsep_custom(char **stringp, const char *delim)
{
    register char *s;
    register const char *spanp;
    register int c, sc;
    char *tok;
    if ((s = *stringp) == NULL)
        return (NULL);
    for (tok = s; ; ) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return (tok);
            }
        } while (sc != 0);
    }
    return 0;
}

int check_compile_error(GLuint shader, const char ** sourceBuffer)
{
    // Get error log size and print it eventually
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char * log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, &logLength, log);
        char *token, *string;
        string = strdup(sourceBuffer[0]);
        int lc = 0;
        while ((token = strsep_custom(&string, "\n")) != NULL) {
           printf("%3d : %s\n", lc, token);
           ++lc;
        }
        fprintf(stderr, "Compile : %s", log);
        delete[] log;
    }
    // If an error happend quit
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
        return -1;     
    return 0;
}

int check_link_error(GLuint program)
{
    // Get link error log size and print it eventually
    int logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char * log = new char[logLength];
        glGetProgramInfoLog(program, logLength, &logLength, log);
        fprintf(stderr, "Link : %s \n", log);
        delete[] log;
    }
    int status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);        
    if (status == GL_FALSE)
        return -1;
    return 0;
}

GLuint compile_shader(GLenum shaderType, const char * sourceBuffer, int bufferSize)
{
    GLuint shaderObject = glCreateShader(shaderType);
    const char * sc[1] = { sourceBuffer };
    glShaderSource(shaderObject, 
                   1, 
                   sc,
                   NULL);
    glCompileShader(shaderObject);
    check_compile_error(shaderObject, sc);
    return shaderObject;
}

GLuint compile_shader_from_file(GLenum shaderType, const char * path)
{
    FILE * shaderFileDesc = fopen( path, "rb" );
    if (!shaderFileDesc)
        return 0;
    fseek ( shaderFileDesc , 0 , SEEK_END );
    long fileSize = ftell ( shaderFileDesc );
    rewind ( shaderFileDesc );
    char * buffer = new char[fileSize + 1];
    long t = fread( buffer, 1, fileSize, shaderFileDesc );
    buffer[fileSize] = '\0';
    GLuint shaderObject = compile_shader(shaderType, buffer, fileSize );
    delete[] buffer;
    if (t != fileSize)
        return -1;
    return shaderObject;
}


bool checkError(const char* title)
{
    int error;
    if((error = glGetError()) != GL_NO_ERROR)
    {
        std::string errorString;
        switch(error)
        {
        case GL_INVALID_ENUM:
            errorString = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errorString = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errorString = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errorString = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            errorString = "GL_OUT_OF_MEMORY";
            break;
        default:
            errorString = "UNKNOWN";
            break;
        }
        fprintf(stdout, "OpenGL Error(%s): %s\n", errorString.c_str(), title);
    }
    return error == GL_NO_ERROR;
}

void camera_compute(Camera & c)
{
    c.eye.x = cos(c.theta) * sin(c.phi) * c.radius + c.o.x;   
    c.eye.y = cos(c.phi) * c.radius + c.o.y ;
    c.eye.z = sin(c.theta) * sin(c.phi) * c.radius + c.o.z;   
    c.up = glm::vec3(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
}

void camera_defaults(Camera & c)
{
    c.phi = 1.0;
    c.theta = 2.3;
    c.radius = 10.f;
    camera_compute(c);
}

void camera_zoom(Camera & c, float factor)
{
    c.radius += factor * c.radius ;
    if (c.radius < 0.1)
    {
        c.radius = 10.f;
        c.o = c.eye + glm::normalize(c.o - c.eye) * c.radius;
    }
    camera_compute(c);
}

void camera_turn(Camera & c, float phi, float theta)
{
    c.theta += 1.f * theta;
    c.phi   -= 1.f * phi;
    if (c.phi >= (2 * M_PI) - 0.1 )
        c.phi = 0.00001;
    else if (c.phi <= 0 )
        c.phi = 2 * M_PI - 0.1;
    camera_compute(c);
}

void camera_pan(Camera & c, float x, float y)
{
    glm::vec3 up(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
    glm::vec3 fwd = glm::normalize(c.o - c.eye);
    glm::vec3 side = glm::normalize(glm::cross(fwd, up));
    c.up = glm::normalize(glm::cross(side, fwd));
    c.o[0] += up[0] * y * c.radius * 2;
    c.o[1] += up[1] * y * c.radius * 2;
    c.o[2] += up[2] * y * c.radius * 2;
    c.o[0] -= side[0] * x * c.radius * 2;
    c.o[1] -= side[1] * x * c.radius * 2;
    c.o[2] -= side[2] * x * c.radius * 2;       
    camera_compute(c);
}

void init_gui_states(GUIStates & guiStates)
{
    guiStates.panLock = false;
    guiStates.turnLock = false;
    guiStates.zoomLock = false;
    guiStates.lockPositionX = 0;
    guiStates.lockPositionY = 0;
    guiStates.camera = 0;
    guiStates.time = 0.0;
    guiStates.playing = false;
    guiStates.display = false;
    guiStates.blit = false;
    guiStates.scroll = 0;
}
