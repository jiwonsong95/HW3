#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <FreeImage/FreeImage.h>
#include <ctime>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Shaders/LoadShaders.h"
#include "My_Shading.h"

//================= 매크로 & 전역 변수 ===================
#define TO_RADIAN 0.01745329252f
#define BUFFER_OFFSET(offset) ((GLvoid *)(offset))

#define LOC_VERTEX   0
#define LOC_NORMAL   1
#define LOC_TEXCOORD 2

// 텍스처 ID
#define N_TEXTURES_USED 5
#define TEXTURE_ID_FLOOR  0
#define TEXTURE_ID_TIGER  1
#define TEXTURE_ID_GRASS  2
#define TEXTURE_ID_BIRDS  3
#define TEXTURE_ID_APPLES 4
GLuint texture_names[N_TEXTURES_USED];

// FBO for G-Buffer
GLuint gBufferFBO;
GLuint gPosition, gNormal, gAlbedo;
GLuint gDepth; // depth buffer for G-Buffer

// 셰이더 프로그램
GLuint h_ShaderProgram_DeferredGeom;     // Geometry Pass
GLuint h_ShaderProgram_DeferredLighting; // Lighting Pass
GLuint h_ShaderProgram_simple;           // (optional) simple

// Geometry Pass 셰이더용 uniform location
GLint loc_g_geom_ModelViewMatrix, loc_g_geom_ModelViewMatrixInvTrans;
GLint loc_g_geom_ModelViewProjectionMatrix;
GLint loc_g_geom_base_texture;

// Lighting Pass 셰이더용 uniform location
GLint loc_lighting_gPosition, loc_lighting_gNormal, loc_lighting_gAlbedo;
GLint loc_numLights;


_loc_light loc_lights[128];

// simple 셰이더용 (axes 등)
GLint loc_simple_MVP;

// 행렬
glm::mat4 ViewMatrix, ProjectionMatrix;

// 호랑이 애니메이션
unsigned int timestamp_scene = 0;
int cur_frame_tiger = 0;
float rotation_angle_tiger = 0.0f;
int flag_tiger_animation = 1;

// 벤치마크용
int frame_count = 0;
const int start_frame = 2;
const int measure_frames = 100;
double total_render_time = 0.0;
bool measuring = false;
std::chrono::high_resolution_clock::time_point frame_start, frame_end;

const int LIGHT_CNT = 32; // 원하는 만큼 설정
static glm::vec4 g_worldLightPos[LIGHT_CNT];
static glm::vec3 g_lightColor[LIGHT_CNT];

static const glm::vec3 palette[] = {
    glm::vec3(1.0f, 0.0f, 0.0f),  // Red
    glm::vec3(0.0f, 1.0f, 0.0f),  // Green
    glm::vec3(0.0f, 0.0f, 1.0f),  // Blue
    glm::vec3(1.0f, 0.0f, 1.0f),  // Magenta
    glm::vec3(0.0f, 1.0f, 1.0f),  // Cyan
    glm::vec3(1.0f, 1.0f, 0.0f),  // Yellow
    glm::vec3(1.0f, 1.0f, 1.0f)   // White
};

const int palette_size = sizeof(palette) / sizeof(palette[0]);

//================= 모델/VAO 관련 ===================

// (1) 바닥
GLuint rectangle_VBO, rectangle_VAO;
#define TEX_COORD_EXTENT 1.0f
GLfloat rectangle_vertices[6][8] = {
    { 0.0f, 0.0f, 0.0f,  0.0f,0.0f, 1.0f,  0.0f,0.0f },
    { 1.0f, 0.0f, 0.0f,  0.0f,0.0f, 1.0f,  TEX_COORD_EXTENT,0.0f },
    { 1.0f, 1.0f, 0.0f,  0.0f,0.0f, 1.0f,  TEX_COORD_EXTENT,TEX_COORD_EXTENT },
    { 0.0f, 0.0f, 0.0f,  0.0f,0.0f, 1.0f,  0.0f,0.0f },
    { 1.0f, 1.0f, 0.0f,  0.0f,0.0f, 1.0f,  TEX_COORD_EXTENT,TEX_COORD_EXTENT },
    { 0.0f, 1.0f, 0.0f,  0.0f,0.0f, 1.0f,  0.0f,TEX_COORD_EXTENT }
};

void prepare_floor(void) {
    glGenBuffers(1, &rectangle_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, rectangle_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectangle_vertices),
        rectangle_vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &rectangle_VAO);
    glBindVertexArray(rectangle_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, rectangle_VBO);
    glVertexAttribPointer(LOC_VERTEX, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 텍스처 설정은 prepare_textures()에서 수행
}

void draw_floor(void) {
    glBindVertexArray(rectangle_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// (2) 호랑이
#define N_TIGER_FRAMES 12
GLuint tiger_VBO, tiger_VAO;
int   tiger_n_triangles[N_TIGER_FRAMES];
int   tiger_vertex_offset[N_TIGER_FRAMES];
GLfloat* tiger_vertices[N_TIGER_FRAMES];

int read_geometry(GLfloat** object, int bytes_per_primitive, char* filename) {
    int n_triangles;
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open the object file %s\n", filename);
        return -1;
    }
    fread(&n_triangles, sizeof(int), 1, fp);

    (*object) = (float*)malloc(n_triangles * bytes_per_primitive);
    fread(*object, bytes_per_primitive, n_triangles, fp);
    fclose(fp);
    return n_triangles;
}

void prepare_tiger(void) {
    int i, n_bytes_per_vertex, n_bytes_per_triangle;
    int tiger_n_total_triangles = 0;
    char filename[256];

    n_bytes_per_vertex = 8 * sizeof(float);
    n_bytes_per_triangle = 3 * n_bytes_per_vertex;

    for (i = 0; i < N_TIGER_FRAMES; i++) {
        sprintf(filename, "Data/Tiger_%d%d_triangles_vnt.geom", i / 10, i % 10);
        tiger_n_triangles[i] = read_geometry(&tiger_vertices[i], n_bytes_per_triangle, filename);
        if (i == 0) tiger_vertex_offset[i] = 0;
        else tiger_vertex_offset[i] = tiger_vertex_offset[i - 1] + 3 * tiger_n_triangles[i - 1];
        tiger_n_total_triangles += tiger_n_triangles[i];
    }

    glGenBuffers(1, &tiger_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, tiger_VBO);
    glBufferData(GL_ARRAY_BUFFER, tiger_n_total_triangles * n_bytes_per_triangle,
        NULL, GL_STATIC_DRAW);

    // 각 프레임 데이터를 큰 VBO에 SubData로 복사
    for (i = 0; i < N_TIGER_FRAMES; i++) {
        glBufferSubData(GL_ARRAY_BUFFER,
            tiger_vertex_offset[i] * n_bytes_per_vertex,
            tiger_n_triangles[i] * n_bytes_per_triangle,
            tiger_vertices[i]);
        free(tiger_vertices[i]);
    }

    glGenVertexArrays(1, &tiger_VAO);
    glBindVertexArray(tiger_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, tiger_VBO);

    glVertexAttribPointer(LOC_VERTEX, 3, GL_FLOAT, GL_FALSE, n_bytes_per_vertex, (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, n_bytes_per_vertex, (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, n_bytes_per_vertex, (GLvoid*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void draw_tiger(void) {
    glBindVertexArray(tiger_VAO);
    glDrawArrays(GL_TRIANGLES,
        tiger_vertex_offset[cur_frame_tiger],
        3 * tiger_n_triangles[cur_frame_tiger]);
    glBindVertexArray(0);
}

TigerInstance g_tigerTable[32] = {
    // -------------------- 첫 16마리 --------------------
    // 1) texture = TIGER, set_material_tiger() = true
    { TEXTURE_ID_TIGER,  true,  glm::vec3(200.0f,   0.0f,   0.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_TIGER,  true,  glm::vec3(-200.0f,   0.0f,   0.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_TIGER,  true,  glm::vec3(100.0f,   0.0f,   0.0f),  glm::vec3(1.0f) },

    // 4) texture = GRASS, no set_material_tiger()
    { TEXTURE_ID_GRASS, false,  glm::vec3(-100.0f,   0.0f,   0.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f,   0.0f, 200.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f,   0.0f,-200.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f,   0.0f, 100.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f,   0.0f,-100.0f),  glm::vec3(1.0f) },

    // 9) texture = BIRDS, scale(0.5)
    { TEXTURE_ID_BIRDS, false,  glm::vec3(200.0f,   0.0f,  200.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_BIRDS, false,  glm::vec3(-200.0f,   0.0f, -200.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_BIRDS, false,  glm::vec3(100.0f,   0.0f,  100.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_BIRDS, false,  glm::vec3(-100.0f,   0.0f, -100.0f), glm::vec3(0.5f) },

    // 13) texture = APPLES, translate(...), scale(0.5)
    { TEXTURE_ID_APPLES,false,  glm::vec3(-150.0f,  50.0f, -150.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_APPLES,false,  glm::vec3(150.0f,  50.0f, -150.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_APPLES,false,  glm::vec3(150.0f,  50.0f, -150.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_APPLES,false,  glm::vec3(-150.0f,  50.0f, -150.0f), glm::vec3(0.5f) }, // 똑같은 것 의도함 : 샘플코드랑 똑같이

    // -------------------- 다음 16마리(두 번째 블록) --------------------
    // 1) texture = TIGER, set_material_tiger() = true
    { TEXTURE_ID_TIGER,  true,  glm::vec3(200.0f, 100.0f,   0.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_TIGER,  true,  glm::vec3(-200.0f, 100.0f,   0.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_TIGER,  true,  glm::vec3(100.0f, 100.0f,   0.0f),  glm::vec3(1.0f) },

    // GRASS
    { TEXTURE_ID_GRASS, false,  glm::vec3(-100.0f, 100.0f,   0.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f, 100.0f, 200.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f, 100.0f,-200.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f, 100.0f, 100.0f),  glm::vec3(1.0f) },
    { TEXTURE_ID_GRASS, false,  glm::vec3(0.0f, 100.0f,-100.0f),  glm::vec3(1.0f) },

    // BIRDS
    { TEXTURE_ID_BIRDS, false,  glm::vec3(200.0f, 100.0f, 200.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_BIRDS, false,  glm::vec3(-200.0f, 100.0f,-200.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_BIRDS, false,  glm::vec3(100.0f, 100.0f, 100.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_BIRDS, false,  glm::vec3(-100.0f, 100.0f,-100.0f), glm::vec3(0.5f) },

    // APPLES
    { TEXTURE_ID_APPLES,false,  glm::vec3(-150.0f,150.0f, -150.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_APPLES,false,  glm::vec3(150.0f,150.0f, -150.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_APPLES,false,  glm::vec3(150.0f,150.0f, -150.0f), glm::vec3(0.5f) },
    { TEXTURE_ID_APPLES,false,  glm::vec3(-150.0f,150.0f, -150.0f), glm::vec3(0.5f) }, // 똑같은 것 의도함 : 샘플코드랑 똑같이
};

void draw_tiger_transformed(const TigerInstance& t, float extraRotX_deg = -90.0f) {
    // [1] 텍스처 바인딩 (디퍼드 셰이딩 지오메트리 패스)
    //     set_material_tiger() 호출 제거
    //     대신 glActiveTexture + glBindTexture로 원하는 텍스처를 바인딩
    glActiveTexture(GL_TEXTURE0 + t.textureID);
    glBindTexture(GL_TEXTURE_2D, texture_names[t.textureID]);
    glUniform1i(loc_g_geom_base_texture, t.textureID);

    // [2] 모델 변환 행렬 생성
    glm::mat4 ModelViewMatrix = glm::rotate(ViewMatrix,
        -rotation_angle_tiger,
        glm::vec3(0.0f, 1.0f, 0.0f));

    ModelViewMatrix = glm::translate(ModelViewMatrix, t.translation);

    if (extraRotX_deg != 0.0f)
        ModelViewMatrix = glm::rotate(ModelViewMatrix,
            extraRotX_deg * TO_RADIAN,
            glm::vec3(1.0f, 0.0f, 0.0f));

    if (t.scale != glm::vec3(1.0f))
        ModelViewMatrix = glm::scale(ModelViewMatrix, t.scale);

    // [3] MVP / Normal Matrix 계산
    glm::mat4 ModelViewProjectionMatrix = ProjectionMatrix * ModelViewMatrix;
    glm::mat3 ModelViewMatrixInvTrans = glm::inverseTranspose(glm::mat3(ModelViewMatrix));

    // [4] 셰이더 uniform 업로드
    glUniformMatrix4fv(loc_g_geom_ModelViewProjectionMatrix, 1, GL_FALSE, &ModelViewProjectionMatrix[0][0]);
    glUniformMatrix4fv(loc_g_geom_ModelViewMatrix, 1, GL_FALSE, &ModelViewMatrix[0][0]);
    glUniformMatrix3fv(loc_g_geom_ModelViewMatrixInvTrans, 1, GL_FALSE, &ModelViewMatrixInvTrans[0][0]);

    // [5] 실제 그리기
    draw_tiger();
}

//================= 전체 화면 사각형(라이팅 패스 용) ===================
GLuint quadVAO = 0, quadVBO = 0;
void prepare_fullscreen_quad() {
    // 간단히 두 개의 삼각형으로 screen-filling quad를 구성
    GLfloat quadVertices[] = {
        // positions    // texCoords
        -1.0f, -1.0f,   0.0f,  0.0f,
         1.0f, -1.0f,   1.0f,  0.0f,
         1.0f,  1.0f,   1.0f,  1.0f,

        -1.0f, -1.0f,   0.0f,  0.0f,
         1.0f,  1.0f,   1.0f,  1.0f,
        -1.0f,  1.0f,   0.0f,  1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // layout(location=0) in vec2 quadPos;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);

    // layout(location=1) in vec2 quadTexCoord;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
        (GLvoid*)(2 * sizeof(GLfloat)));

    glBindVertexArray(0);
}

//================= 텍스처 로드 함수 ===================
void My_glTexImage2D_from_file(char* filename) {
    FREE_IMAGE_FORMAT tx_file_format = FreeImage_GetFileType(filename, 0);
    FIBITMAP* tx_pixmap = FreeImage_Load(tx_file_format, filename);
    FIBITMAP* tx_pixmap_32;
    int tx_bits_per_pixel = FreeImage_GetBPP(tx_pixmap);

    if (tx_bits_per_pixel == 32) tx_pixmap_32 = tx_pixmap;
    else {
        tx_pixmap_32 = FreeImage_ConvertTo32Bits(tx_pixmap);
        FreeImage_Unload(tx_pixmap);
    }

    int width = FreeImage_GetWidth(tx_pixmap_32);
    int height = FreeImage_GetHeight(tx_pixmap_32);
    void* data = FreeImage_GetBits(tx_pixmap_32);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    FreeImage_Unload(tx_pixmap_32);
}

//================= G-Buffer 생성 함수 ===================
void createGBuffer(int width, int height) {
    // FBO
    glGenFramebuffers(1, &gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

    // (1) Position
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // (2) Normal
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // (3) Albedo
    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

    // (4) Depth
    glGenTextures(1, &gDepth);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);

    // MRT
    GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("GBuffer FBO not complete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//================= 셰이더 로딩 ===================
void prepare_shader_programs(void) {
    // (a) Geometry Pass 셰이더
    ShaderInfo geom_shader_info[] = {
        { GL_VERTEX_SHADER,   "Shaders/DeferredGeometry.vert" },
        { GL_FRAGMENT_SHADER, "Shaders/DeferredGeometry.frag" },
        { GL_NONE, NULL }
    };
    h_ShaderProgram_DeferredGeom = LoadShaders(geom_shader_info);

    loc_g_geom_ModelViewMatrix = glGetUniformLocation(h_ShaderProgram_DeferredGeom, "u_ModelViewMatrix");
    loc_g_geom_ModelViewMatrixInvTrans = glGetUniformLocation(h_ShaderProgram_DeferredGeom, "u_ModelViewMatrixInvTrans");
    loc_g_geom_ModelViewProjectionMatrix = glGetUniformLocation(h_ShaderProgram_DeferredGeom, "u_ModelViewProjectionMatrix");
    loc_g_geom_base_texture = glGetUniformLocation(h_ShaderProgram_DeferredGeom, "u_base_texture");

    // (b) Lighting Pass 셰이더
    ShaderInfo light_shader_info[] = {
        { GL_VERTEX_SHADER,   "Shaders/DeferredLighting.vert" },
        { GL_FRAGMENT_SHADER, "Shaders/DeferredLighting.frag" },
        { GL_NONE, NULL }
    };
    h_ShaderProgram_DeferredLighting = LoadShaders(light_shader_info);

    loc_lighting_gPosition = glGetUniformLocation(h_ShaderProgram_DeferredLighting, "gPosition");
    loc_lighting_gNormal = glGetUniformLocation(h_ShaderProgram_DeferredLighting, "gNormal");
    loc_lighting_gAlbedo = glGetUniformLocation(h_ShaderProgram_DeferredLighting, "gAlbedo");

    loc_numLights = glGetUniformLocation(h_ShaderProgram_DeferredLighting, "numLights");

    // 광원 uniform location 잡기
    for (int i = 0; i < 128; i++) {
        char buf[128];
        sprintf(buf, "lights[%d].Position", i);
        loc_lights[i].position = glGetUniformLocation(h_ShaderProgram_DeferredLighting, buf);

        sprintf(buf, "lights[%d].Color", i);
        loc_lights[i].color = glGetUniformLocation(h_ShaderProgram_DeferredLighting, buf);

        sprintf(buf, "lights[%d].Linear", i);
        loc_lights[i].linear = glGetUniformLocation(h_ShaderProgram_DeferredLighting, buf);

        sprintf(buf, "lights[%d].Quadratic", i);
        loc_lights[i].quadratic = glGetUniformLocation(h_ShaderProgram_DeferredLighting, buf);
    }
}

//================= 디스플레이 함수 (핵심) ===================
void display(void) {
    // 벤치마크 start
    if (frame_count >= start_frame && frame_count < start_frame + measure_frames) {
        frame_start = std::chrono::high_resolution_clock::now();
        measuring = true;
    }

    //--------------- [1] Geometry Pass ---------------
    glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(h_ShaderProgram_DeferredGeom);

    // (1) 바닥
    {
        glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_FLOOR);
        glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_FLOOR]);
        glUniform1i(loc_g_geom_base_texture, TEXTURE_ID_FLOOR);

        glm::mat4 ModelMatrix = glm::mat4(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, glm::vec3(-500.0f, 0.0f, 500.0f));
        ModelMatrix = glm::scale(ModelMatrix, glm::vec3(1000.0f, 1000.0f, 1000.0f));
        ModelMatrix = glm::rotate(ModelMatrix, -90.0f * TO_RADIAN, glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4 MV = ViewMatrix * ModelMatrix;
        glm::mat3 MVinv = glm::inverseTranspose(glm::mat3(MV));
        glm::mat4 MVP = ProjectionMatrix * MV;

        glUniformMatrix4fv(loc_g_geom_ModelViewMatrix, 1, GL_FALSE, &MV[0][0]);
        glUniformMatrix3fv(loc_g_geom_ModelViewMatrixInvTrans, 1, GL_FALSE, &MVinv[0][0]);
        glUniformMatrix4fv(loc_g_geom_ModelViewProjectionMatrix, 1, GL_FALSE, &MVP[0][0]);

        draw_floor();
    }

    // (2) 호랑이 32마리 그리기
    for (auto& t : g_tigerTable) {
        draw_tiger_transformed(t);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    //--------------- [2] Lighting Pass ---------------
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(h_ShaderProgram_DeferredLighting);

    // G-Buffer 텍스처 3개 bind
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glUniform1i(loc_lighting_gPosition, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glUniform1i(loc_lighting_gNormal, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glUniform1i(loc_lighting_gAlbedo, 2);

    glUniform1i(loc_numLights, LIGHT_CNT);

    // 저장된 광원 가져오기
    for (int i = 0; i < LIGHT_CNT; i++) {
        glm::vec4 viewLightPos = ViewMatrix * g_worldLightPos[i];

        glUniform3f(loc_lights[i].position,
            viewLightPos.x, viewLightPos.y, viewLightPos.z);

        glm::vec3 c = g_lightColor[i];
        glUniform3f(loc_lights[i].color, c.r, c.g, c.b);

        glUniform1f(loc_lights[i].linear, 0.15f);
        glUniform1f(loc_lights[i].quadratic, 0.0005f);
    }

    // (화면 전체 사각형) 그리기: VAO로
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);

    //--------------- SwapBuffers ---------------
    glutSwapBuffers();

    // 벤치마크 end
    if (measuring) {
        frame_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> frame_time = frame_end - frame_start;
        total_render_time += frame_time.count();

        if (++frame_count == start_frame + measure_frames) {
            double average = total_render_time / measure_frames;
            printf("Average: %.3f ms\n", average);
            measuring = false;
        }
    }
    else {
        frame_count++;
    }
}

//================= 타이머 & 콜백 ===================
void timer_scene(int value) {
    timestamp_scene = (timestamp_scene + 1) % UINT_MAX;
    cur_frame_tiger = timestamp_scene % N_TIGER_FRAMES;
    rotation_angle_tiger = (timestamp_scene % 360) * TO_RADIAN;

    glutPostRedisplay();
    if (flag_tiger_animation) glutTimerFunc(100, timer_scene, 0);
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    float aspect = (float)width / (float)height;
    ProjectionMatrix = glm::perspective(45.0f * TO_RADIAN, aspect, 1.0f, 20000.0f);

    // GBuffer도 크기 재생성 (화면 크기 변하면)
    createGBuffer(width, height);
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'a':
        flag_tiger_animation = 1 - flag_tiger_animation;
        if (flag_tiger_animation) glutTimerFunc(100, timer_scene, 0);
        break;
    case 27:
        glutLeaveMainLoop();
        break;
    }
}

//================= 초기화 루틴 ===================
void initialize_OpenGL(void) {
    glewExperimental = GL_TRUE;
    GLenum error = glewInit();
    if (error != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
        exit(-1);
    }
    printf("GL_VERSION : %s\n", glGetString(GL_VERSION));
    printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));

    glEnable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);

    // ViewMatrix 예시
    ViewMatrix = glm::lookAt(glm::vec3(500.0f, 400.0f, 500.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
}

//================= 씬 준비 ===================
void prepare_textures(void) {
    glGenTextures(N_TEXTURES_USED, texture_names);

    // Floor
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_FLOOR);
    glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_FLOOR]);
    My_glTexImage2D_from_file("Data/checker_tex.jpg");

    // Tiger
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_TIGER);
    glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_TIGER]);
    My_glTexImage2D_from_file("Data/tiger_tex2.jpg");

    // Birds
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_BIRDS);
    glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_BIRDS]);
    My_glTexImage2D_from_file("Data/Image_4_6304_4192.jpg");

    // Grass
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_GRASS);
    glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_GRASS]);
    My_glTexImage2D_from_file("Data/grass_tex.jpg");

    // Apple
    glActiveTexture(GL_TEXTURE0 + TEXTURE_ID_APPLES);
    glBindTexture(GL_TEXTURE_2D, texture_names[TEXTURE_ID_APPLES]);
    My_glTexImage2D_from_file("Data/apples.jpg");
}

void prepare_scene(void) {
    // 모델/VAO 준비
    prepare_floor();
    prepare_tiger();

    // 텍스처 로딩
    prepare_textures();

    // 풀 스크린 사각형(라이팅 패스용) VAO 준비
    prepare_fullscreen_quad();

    // G-Buffer 생성 (초기 창 크기 가정)
    createGBuffer(800, 800);

    // Light 준비
    for (int i = 0; i < LIGHT_CNT; i++) {
        float wx = (float)(std::rand() % 501 - 250);
        float wy = (float)(std::rand() % 201);
        float wz = (float)(std::rand() % 501 - 250);

        g_worldLightPos[i] = glm::vec4(wx, wy, wz, 1.0f);

        int color_index = std::rand() % palette_size;
        g_lightColor[i] = palette[color_index];
    }
}

//================= 메인 ===================
int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(800, 800);
    glutInitContextVersion(4, 5);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutCreateWindow("Deferred Shading Example");

    initialize_OpenGL();

    // 셰이더 준비
    prepare_shader_programs();

    // 씬 준비
    prepare_scene();

    // 콜백 등록
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(100, timer_scene, 0);

    glutMainLoop();

    return 0;
}
