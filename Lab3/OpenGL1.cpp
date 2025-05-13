#define STB_IMAGE_IMPLEMENTATION 
#include "stb_image.h"         

#include <iostream>
#include <vector>
#include <string> 
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

// В программе реализована следующая интерактивность:
//  Перемещение камеры на WASD Space Shift
//  Поворот камеры на стрелочки ВВЕРХ/ВНИЗ/ВЛЕВО/ВПРАВО
//  Вращение поверхности на F
//  Изменение коэффициента смешивания текстур: Q / E

// --- Глобальные настройки ---

// Параметры окна
const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;
const char* WINDOW_TITLE = "Textured Illuminated Tessellated Surface";

// Параметры поверхности
const int GRID_SIZE = 63; // 64x64 patches
const float PLANE_SIZE = 4.0f; // Размер квадратной плоскости (от -PLANE_SIZE/2 до +PLANE_SIZE/2)
const float SIN_AMPLITUDE = 0.2f; // Амплитуда синусоиды
const float SIN_FREQUENCY = glm::pi<float>() * 2.0f; // Частота синусоиды

// Параметры тесселяции
const float TESS_LEVEL_INNER = (float)GRID_SIZE / 4.0f; // Уровень внутренней тесселяции
const float TESS_LEVEL_OUTER = (float)GRID_SIZE / 4.0f; // Уровень внешней тесселяции

// Параметры освещения (Блинн-Фонг)
const glm::vec3 LIGHT_POS = glm::vec3(3.0f, 3.0f, 3.0f);
const glm::vec3 LIGHT_COLOR = glm::vec3(1.0f, 1.0f, 1.0f); // Белый свет
const glm::vec3 MATERIAL_AMBIENT = glm::vec3(0.1f);
// const glm::vec3 MATERIAL_DIFFUSE = glm::vec3(0.8f, 0.8f, 0.8f); // Базовый цвет теперь берется из текстур
const glm::vec3 MATERIAL_SPECULAR = glm::vec3(0.9f, 0.9f, 0.9f); // Цвет блика
const float MATERIAL_SHININESS = 1.0f; // Экспонента блеска 

// Параметры текстур
const std::string TEXTURE_PATH_1 = "C:\\Users\\UTAI Jr\\source\\repos\\OpenGL1\\OpenGL1\\cat.jpg"; // Путь к первой текстуре
const std::string TEXTURE_PATH_2 = "C:\\Users\\UTAI Jr\\source\\repos\\OpenGL1\\OpenGL1\\amogus.png"; // Путь к второй текстуре
float g_blendFactor = 0.0f; // Коэффициент смешивания текстур
float g_blendFactorChangeSpeed = 0.5f; // Скорость изменения коэффициента смешивания

// Параметры преобразований модели
glm::vec3 g_modelTranslation = glm::vec3(0.0f, 0.0f, 0.0f); // Смещение модели
glm::vec3 g_modelScale = glm::vec3(1.0f, 1.0f, 1.0f); // Масштаб модели
// Угол поворота для анимации (вокруг Z)
float g_rotationAngleZ = 0.0f;

// Параметры камеры
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, -7.0f); // Начальная позиция камеры
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f); // Будет рассчитано из yaw/pitch
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // Вектор "верха"

// Углы Эйлера (рыскание и тангаж) для камеры
float yaw = 90.0f;
float pitch = 0.0f;


// Переменные для плавного расчета времени
double g_lastTime = 0.0;
float g_deltaTime = 0.0f;

// Скорость камеры
float cameraSpeed = 3.0f; // Скорость перемещения
float sensitivity = 80.0f; // Чувствительность поворота стрелками (градусов в секунду)

// Переменные для анимации вращения модели
bool g_rotate = false;
float g_objectRotationSpeedRad = glm::radians(45.0f); // Скорость вращения объекта (радиан в секунду)


// --- Структура для объекта OpenGL ---
GLFWwindow* g_window = nullptr;

struct Object {
    GLuint vbo = 0, ibo = 0, vao = 0;
    GLsizei indexCount = 0;
    GLuint shaderProgram = 0;
    GLuint texture1 = 0; // ID текстуры 1
    GLuint texture2 = 0; // ID текстуры 2

    // Uniform locations
    GLint u_Model = -1;
    GLint u_NormalMatrix = -1;
    GLint u_VP = -1; // View * Projection matrix
    GLint u_LightPos = -1;
    GLint u_ViewPos = -1;

    GLint u_AmbientColor = -1;
    // GLint u_DiffuseColor = -1; // Удалено, так как диффузный цвет теперь из текстур
    GLint u_SpecularColor = -1;
    GLint u_Shininess = -1;
    GLint u_LightColor = -1;

    GLint u_TessLevelInner = -1;
    GLint u_TessLevelOuter = -1;

    // Новые Uniform locations для текстур и смешивания
    GLint u_Texture1 = -1;
    GLint u_Texture2 = -1;
    GLint u_BlendFactor = -1;
};

Object g_object;


// --- Функции компиляции шейдеров ---
GLuint createShader(const GLchar* code, GLenum type) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &code, NULL);
    glCompileShader(id);

    GLint compiled;
    glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            std::vector<char> log(len);
            glGetShaderInfoLog(id, len, NULL, log.data());
            cerr << "Shader compile error (" << (type == GL_VERTEX_SHADER ? "Vertex" : (type == GL_FRAGMENT_SHADER ? "Fragment" : (type == GL_TESS_CONTROL_SHADER ? "Tess Control" : (type == GL_TESS_EVALUATION_SHADER ? "Tess Eval" : "Unknown")))) << "):" << endl << log.data() << endl;
        }
        else {
            cerr << "Shader compile error: No info log available." << endl;
        }
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint createProgram(GLuint vS, GLuint tcS, GLuint teS, GLuint fS) {
    GLuint id = glCreateProgram();
    glAttachShader(id, vS);
    if (tcS) glAttachShader(id, tcS);
    if (teS) glAttachShader(id, teS);
    glAttachShader(id, fS);
    glLinkProgram(id);

    glDetachShader(id, vS);
    glDeleteShader(vS);
    if (tcS) { glDetachShader(id, tcS); glDeleteShader(tcS); }
    if (teS) { glDetachShader(id, teS); glDeleteShader(teS); }
    glDetachShader(id, fS);
    glDeleteShader(fS);


    GLint linked;
    glGetProgramiv(id, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            std::vector<char> log(len);
            glGetProgramInfoLog(id, len, NULL, log.data());
            cerr << "Program link error: " << endl << log.data() << endl;
        }
        else {
            cerr << "Program link error: No info log available." << endl;
        }
        glDeleteProgram(id);
        return 0;
    }
    return id;
}

// --- Функция загрузки текстуры ---
GLuint loadTexture(const std::string& path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // Указываем stb_image, что нужно перевернуть изображение по вертикали при загрузке
    // т.к. OpenGL ожидает координату 0.0 по оси Y внизу текстуры, а изображения обычно имеют 0.0 наверху.
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
            cerr << "Error loading texture '" << path << "': Unsupported number of components (" << nrComponents << ")" << endl;
            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); // Генерация мипмапов

        // Установка параметров текстуры
        // Трилинейная фильтрация (GL_LINEAR_MIPMAP_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Трилинейная для минимизации
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Билинейная для увеличения

        // Анизотропная фильтрация (если поддерживается)
        if (glewIsSupported("GL_EXT_texture_filter_anisotropic")) {
            GLfloat maxAnisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
            cout << "Anisotropic filtering enabled for '" << path << "' with max level: " << maxAnisotropy << endl;
        }
        else {
            cout << "Anisotropic filtering not supported." << endl;
        }


        stbi_image_free(data); // Освобождаем память изображения
        cout << "Texture loaded successfully: '" << path << "' (" << width << "x" << height << ", " << nrComponents << " channels)" << endl;
    }
    else {
        cerr << "Texture failed to load at path: " << path << endl;
        cerr << "STB Error: " << stbi_failure_reason() << endl;
        glDeleteTextures(1, &textureID);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, 0); // Отвязываем текстуру
    return textureID;
}


// --- Шейдеры ---

// Вершинный шейдер (Добавлены текстурные координаты)
const GLchar vsh[] =
"#version 410 core\n" \
"layout(location = 0) in vec3 a_position;\n" \
"layout(location = 1) in vec3 a_normal;\n" \
"layout(location = 2) in vec2 a_texCoord;\n" /* Добавлено */ \
"\n" \
"out VS_OUT {\n" \
"	vec3 localPos; // Позиция в локальных координатах модели\n" \
"	vec3 localNormal; // Нормаль в локальных координатах модели\n" \
"   vec2 texCoord; // Текстурные координаты \n" /* Добавлено */ \
"} vs_out;\n" \
"\n" \
"void main() {\n" \
"	vs_out.localPos = a_position;\n" \
"	vs_out.localNormal = a_normal;\n" \
"   vs_out.texCoord = a_texCoord;\n" /* Добавлено */ \
"}\n";

// Тесселяционный контрольный шейдер (Пробрасывает текстурные координаты)
const GLchar tcsh[] =
"#version 410 core\n" \
"layout(vertices = 3) out;\n" \
"\n" \
"in VS_OUT {\n" \
"	vec3 localPos;\n" \
"	vec3 localNormal;\n" \
"   vec2 texCoord;\n" /* Добавлено */ \
"} vs_in[];\n" \
"\n" \
"out TCS_OUT {\n" \
"	vec3 localPos;\n" \
"	vec3 localNormal;\n" \
"   vec2 texCoord;\n" /* Добавлено */ \
"} tcs_out[];\n" \
"\n" \
"// Можно задать через uniform (значения по умолчанию)\n" \
"uniform float u_TessLevelInner = 16.0;\n" \
"uniform float u_TessLevelOuter = 16.0;\n" \
"\n" \
"void main() {\n" \
"	tcs_out[gl_InvocationID].localPos = vs_in[gl_InvocationID].localPos;\n" \
"	tcs_out[gl_InvocationID].localNormal = vs_in[gl_InvocationID].localNormal;\n" \
"   tcs_out[gl_InvocationID].texCoord = vs_in[gl_InvocationID].texCoord;\n" /* Добавлено */ \
"\n" \
"	// Уровни тесселяции устанавливаем только один раз (в вызове 0)\n" \
"	if (gl_InvocationID == 0) {\n" \
"		gl_TessLevelInner[0] = u_TessLevelInner;\n" \
"		gl_TessLevelOuter[0] = u_TessLevelOuter;\n" \
"		gl_TessLevelOuter[1] = u_TessLevelOuter;\n" \
"		gl_TessLevelOuter[2] = u_TessLevelOuter;\n" \
"	}\n" \
"}\n";

// Тесселяционный оценочный шейдер: Интерполирует атрибуты (включая текстурные координаты), вычисляет позицию и нормаль
const GLchar tesh[] =
"#version 410 core\n" \
"layout(triangles, equal_spacing, ccw) in;\n" \
"\n" \
"in TCS_OUT {\n" \
"	vec3 localPos;\n" \
"	vec3 localNormal;\n" \
"   vec2 texCoord;\n" /* Добавлено */ \
"} tcs_in[];\n" \
"\n" \
"out TES_OUT {\n" \
"	vec3 worldPos;\n" \
"	vec3 worldNormal;\n" \
"   vec2 texCoord;\n" /* Добавлено */ \
"} tes_out;\n" \
"\n" \
"uniform mat4 u_model; \n" \
"uniform mat3 u_normalMatrix; \n" \
"uniform mat4 u_vp; \n" \
"\n" \
"vec3 interpolateVec3(vec3 v0, vec3 v1, vec3 v2) {\n" \
"	return vec3(gl_TessCoord.x) * v0 + vec3(gl_TessCoord.y) * v1 + vec3(gl_TessCoord.z) * v2;\n" \
"}\n" \
"\n" \
"vec2 interpolateVec2(vec2 v0, vec2 v1, vec2 v2) {\n" /* Добавлено */ \
"	return vec2(gl_TessCoord.x) * v0 + vec2(gl_TessCoord.y) * v1 + vec2(gl_TessCoord.z) * v2;\n" \
"}\n" \
"\n" \
"void main() {\n" \
"	vec3 localPos = interpolateVec3(tcs_in[0].localPos, tcs_in[1].localPos, tcs_in[2].localPos);\n" \
"	vec3 localNormal = interpolateVec3(tcs_in[0].localNormal, tcs_in[1].localNormal, tcs_in[2].localNormal);\n" \
"   tes_out.texCoord = interpolateVec2(tcs_in[0].texCoord, tcs_in[1].texCoord, tcs_in[2].texCoord);\n" /* Добавлено */ \
"\n" \
"	tes_out.worldPos = vec3(u_model * vec4(localPos, 1.0));\n" \
"\n" \
"	// Нормаль должна быть интерполирована и трансформирована. \n" \
"   // Важно: нормализация происходит после трансформации, чтобы избежать проблем с масштабированием.\n" \
"	tes_out.worldNormal = normalize(u_normalMatrix * normalize(localNormal));\n" \
"\n" \
"	gl_Position = u_vp * vec4(tes_out.worldPos, 1.0);\n" \
"}\n";


// Фрагментный шейдер: Смешивание текстур и расчет освещения по Блинну-Фонга
const GLchar fsh[] =
"#version 410 core\n" \
"in TES_OUT {\n" \
"   vec3 worldPos;\n" \
"   vec3 worldNormal;\n" \
"   vec2 texCoord;\n" /* Добавлено */ \
"} fs_in;\n" \
"\n" \
"out vec4 o_color;\n" \
"\n" \
"uniform vec3 u_lightPos; // Позиция источника света в мировом пространстве\n" \
"uniform vec3 u_lightColor; // Цвет света\n" \
"uniform vec3 u_viewPos; // Позиция камеры в мировом пространстве\n" \
"\n" \
"uniform vec3 u_ambientColor;\n" \
// "uniform vec3 u_diffuseColor;" // Заменено текстурами \n"
"uniform vec3 u_specularColor;\n" \
"uniform float u_shininess;\n" \
"\n" \
"// Новые uniforms для текстур и смешивания\n" \
"uniform sampler2D u_texture1;\n" \
"uniform sampler2D u_texture2;\n" \
"uniform float u_blendFactor; // 0.0 = texture1, 1.0 = texture2\n" \
"\n" \
"void main() {\n" \
"   // Получаем цвета из обеих текстур\n" \
"   vec4 texColor1 = texture(u_texture1, fs_in.texCoord);\n" \
"   vec4 texColor2 = texture(u_texture2, fs_in.texCoord);\n" \
"\n" \
"   // Смешиваем цвета текстур\n" \
"   vec3 diffuseColor = mix(texColor1.rgb, texColor2.rgb, u_blendFactor);\n" \
"\n" \
"	vec3 N = normalize(fs_in.worldNormal);\n" \
"	vec3 L = normalize(u_lightPos - fs_in.worldPos); // Направление к свету\n" \
"   vec3 V = normalize(u_viewPos - fs_in.worldPos); // Направление к камере\n" \
"	vec3 H = normalize(L + V); // Вектор полупути (для Blinn-Phong)\n" \
"\n" \
"	// Расчет освещения (Blinn-Phong)\n" \
"	// Ambient (фоновое освещение)\n" \
"	vec3 ambient = u_ambientColor * u_lightColor;\n" \
"\n" \
"	// Diffuse (рассеянное освещение) - Используем смешанный цвет текстур\n" \
"	float diff = max(dot(N, L), 0.0);\n" \
"	vec3 diffuse = diffuseColor * u_lightColor * diff;\n" \
"\n" \
"	// Specular (блик)\n" \
"	float spec = pow(max(dot(N, H), 0.0), u_shininess);\n" \
"	vec3 specular = u_specularColor * u_lightColor * spec;\n" \
"   // Если основной цвет (из текстур) темный, блик будет менее заметен.\n" \
"   // Можно добавить проверку, чтобы блик был только на светлых участках, но пока оставим так.\n" \
"\n" \
"	// Итоговый цвет\n" \
"	o_color = vec4(ambient + diffuse + specular, 1.0);\n" \
"   // o_color = vec4(diffuseColor, 1.0); // Для отладки текстур\n" \
"}\n";


bool createShaderProgram() {

    GLuint vS = createShader(vsh, GL_VERTEX_SHADER);
    GLuint tcS = createShader(tcsh, GL_TESS_CONTROL_SHADER);
    GLuint teS = createShader(tesh, GL_TESS_EVALUATION_SHADER);
    GLuint fS = createShader(fsh, GL_FRAGMENT_SHADER);


    if (vS == 0 || tcS == 0 || teS == 0 || fS == 0) {
        if (vS) glDeleteShader(vS);
        if (tcS) glDeleteShader(tcS);
        if (teS) glDeleteShader(teS);
        if (fS) glDeleteShader(fS);
        return false;
    }

    g_object.shaderProgram = createProgram(vS, tcS, teS, fS);

    if (g_object.shaderProgram == 0) {
        return false;
    }

    // Получение uniform location для старых uniforms
    g_object.u_Model = glGetUniformLocation(g_object.shaderProgram, "u_model");
    g_object.u_NormalMatrix = glGetUniformLocation(g_object.shaderProgram, "u_normalMatrix");
    g_object.u_VP = glGetUniformLocation(g_object.shaderProgram, "u_vp");
    g_object.u_LightPos = glGetUniformLocation(g_object.shaderProgram, "u_lightPos");
    g_object.u_ViewPos = glGetUniformLocation(g_object.shaderProgram, "u_viewPos");
    g_object.u_LightColor = glGetUniformLocation(g_object.shaderProgram, "u_lightColor");
    g_object.u_AmbientColor = glGetUniformLocation(g_object.shaderProgram, "u_ambientColor");
    // g_object.u_DiffuseColor = glGetUniformLocation(g_object.shaderProgram, "u_diffuseColor"); // Удалено
    g_object.u_SpecularColor = glGetUniformLocation(g_object.shaderProgram, "u_specularColor");
    g_object.u_Shininess = glGetUniformLocation(g_object.shaderProgram, "u_shininess");
    g_object.u_TessLevelInner = glGetUniformLocation(g_object.shaderProgram, "u_TessLevelInner");
    g_object.u_TessLevelOuter = glGetUniformLocation(g_object.shaderProgram, "u_TessLevelOuter");

    // Получение uniform location для новых uniforms текстур
    g_object.u_Texture1 = glGetUniformLocation(g_object.shaderProgram, "u_texture1");
    g_object.u_Texture2 = glGetUniformLocation(g_object.shaderProgram, "u_texture2");
    g_object.u_BlendFactor = glGetUniformLocation(g_object.shaderProgram, "u_blendFactor");


    // Проверка всех uniforms
    bool uniforms_ok = true;
    if (g_object.u_Model == -1) { cerr << "Uniform 'u_model' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_NormalMatrix == -1) { cerr << "Uniform 'u_normalMatrix' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_VP == -1) { cerr << "Uniform 'u_vp' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_LightPos == -1) { cerr << "Uniform 'u_lightPos' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_ViewPos == -1) { cerr << "Uniform 'u_viewPos' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_LightColor == -1) { cerr << "Uniform 'u_lightColor' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_AmbientColor == -1) { cerr << "Uniform 'u_ambientColor' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_SpecularColor == -1) { cerr << "Uniform 'u_specularColor' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_Shininess == -1) { cerr << "Uniform 'u_shininess' not found!" << endl; uniforms_ok = false; }

    // Проверка новых uniforms
    if (g_object.u_Texture1 == -1) { cerr << "Uniform 'u_texture1' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_Texture2 == -1) { cerr << "Uniform 'u_texture2' not found!" << endl; uniforms_ok = false; }
    if (g_object.u_BlendFactor == -1) { cerr << "Uniform 'u_blendFactor' not found!" << endl; uniforms_ok = false; }

    // Опциональные uniforms тесселяции
    if (g_object.u_TessLevelInner == -1) { cout << "Optional uniform 'u_TessLevelInner' not found." << endl; }
    if (g_object.u_TessLevelOuter == -1) { cout << "Optional uniform 'u_TessLevelOuter' not found." << endl; }


    if (!uniforms_ok) {
        cerr << "Failed to get all required uniform locations." << endl;
        glDeleteProgram(g_object.shaderProgram);
        g_object.shaderProgram = 0;
        return false;
    }

    return true;
}

// Функция для расчета Z и нормали для синусоидальной плоскости
void calculateSurfaceData(float x, float y, float& z, glm::vec3& normal) {
    float r_squared = x * x + y * y;
    float r = glm::sqrt(r_squared);

    // Рассчитываем Z = f(x, y)
    z = SIN_AMPLITUDE * glm::sin(SIN_FREQUENCY * r);

    // Рассчитываем нормаль по градиенту (-df/dx, -df/dy, 1)
    glm::vec3 grad;
    if (r > 1e-6f) { // Избегаем деления на ноль в центре
        // Частные производные dz/dx и dz/dy
        float common_term = SIN_AMPLITUDE * SIN_FREQUENCY * glm::cos(SIN_FREQUENCY * r) / r;
        grad.x = common_term * x;
        grad.y = common_term * y;
    }
    else {
        // В центре (0,0) нормаль смотрит строго вверх
        grad.x = 0.0f;
        grad.y = 0.0f;
    }
    // grad.z = 1.0f; // Это было для нормализации вектора (-dz/dx, -dz/dy, 1), но лучше нормализовать в конце

    // Вектор нормали к поверхности z = f(x, y) это (-dz/dx, -dz/dy, 1)
    normal = glm::normalize(glm::vec3(-grad.x, -grad.y, 1.0f));
}


bool createModel() {
    vector<float> vertices;
    vector<unsigned int> indices;

    const float halfSize = PLANE_SIZE * 0.5f;
    const float step = PLANE_SIZE / GRID_SIZE;
    const int numVerticesPerRow = GRID_SIZE + 1;

    // Генерируем вершины, нормали и текстурные координаты для сетки (N+1)x(N+1)
    for (int i = 0; i <= GRID_SIZE; ++i) { // Y (строки)
        for (int j = 0; j <= GRID_SIZE; ++j) { // X (столбцы)
            float x = -halfSize + j * step;
            float y = -halfSize + i * step;
            float z;
            glm::vec3 normal;

            calculateSurfaceData(x, y, z, normal);

            // Текстурные координаты (u, v) отображаются от 0 до 1 по всей плоскости
            float u = (float)j / GRID_SIZE;
            float v = (float)i / GRID_SIZE;

            // Добавляем данные вершины: позиция (3), нормаль (3), текстурные координаты (2)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    // Генерируем индексы для треугольников (патчи по 3 вершины)
    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            unsigned int idx00 = i * numVerticesPerRow + j;       // (i, j)     Bottom-left
            unsigned int idx10 = idx00 + 1;                   // (i, j+1)   Bottom-right
            unsigned int idx01 = idx00 + numVerticesPerRow;     // (i+1, j)   Top-left
            unsigned int idx11 = idx01 + 1;                   // (i+1, j+1) Top-right

            // Треугольник 1 (нижний левый)
            indices.push_back(idx00);
            indices.push_back(idx01);
            indices.push_back(idx10);

            // Треугольник 2 (верхний правый)
            indices.push_back(idx10);
            indices.push_back(idx01);
            indices.push_back(idx11);
        }
    }


    g_object.indexCount = static_cast<GLsizei>(indices.size());
    if (g_object.indexCount == 0) {
        cerr << "Error: No indices generated for the model." << endl;
        return false;
    }

    // Создаем VAO, VBO, IBO
    glGenVertexArrays(1, &g_object.vao);
    glBindVertexArray(g_object.vao);

    glGenBuffers(1, &g_object.vbo);
    glGenBuffers(1, &g_object.ibo);

    // Загружаем данные вершин в VBO
    glBindBuffer(GL_ARRAY_BUFFER, g_object.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Загружаем данные индексов в IBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_object.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Указываем формат вершинных данных (атрибуты)
    const GLsizei stride = 8 * sizeof(float); // 3 pos + 3 normal + 2 texcoord

    // Атрибут 0: Позиция (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // Атрибут 1: Нормаль (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    // Атрибут 2: Текстурные координаты (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));


    // Отвязываем VAO, VBO, IBO
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


    GLenum err;
    if ((err = glGetError()) != GL_NO_ERROR) {
        cerr << "OpenGL error after createModel: " << err << endl;
        // Очистка в случае ошибки
        if (g_object.vao) glDeleteVertexArrays(1, &g_object.vao);
        if (g_object.vbo) glDeleteBuffers(1, &g_object.vbo);
        if (g_object.ibo) glDeleteBuffers(1, &g_object.ibo);
        g_object.vao = g_object.vbo = g_object.ibo = 0;
        return false;
    }

    cout << "Model created successfully with " << vertices.size() / 8 << " vertices and " << indices.size() / 3 << " triangles (" << indices.size() << " indices)." << endl;

    return g_object.vao != 0;
}


void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        g_rotate = !g_rotate;
        if (g_rotate) {
            cout << "Rotation ENABLED" << endl;
        }
        else {
            cout << "Rotation DISABLED" << endl;
        }
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    // Динамическое изменение смешивания текстур больше не здесь, а в processInput
}


bool initApp() {
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST); // Включаем тест глубины
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE); // Включаем отсечение граней
    glCullFace(GL_BACK); // Отбрасываем задние грани

    glfwSetKeyCallback(g_window, keyCallback); // Установка callback для однократных нажатий (F, ESC)

    // Загрузка текстур
    g_object.texture1 = loadTexture(TEXTURE_PATH_1);
    g_object.texture2 = loadTexture(TEXTURE_PATH_2);
    if (g_object.texture1 == 0 || g_object.texture2 == 0) {
        cerr << "Failed to load one or more textures. Ensure the image files exist at the specified paths." << endl;
        // Можно решить, продолжать ли без текстур или выходить
        // return false; // Раскомментировать, если текстуры обязательны
    }


    if (!createShaderProgram()) {
        cerr << "Failed to create shader program!" << endl;
        return false;
    }
    if (!createModel()) {
        cerr << "Failed to create model!" << endl;
        return false;
    }

    // Указываем OpenGL, что мы будем рендерить патчи из 3 вершин
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    // Включить MSAA если было запрошено при создании окна
    glEnable(GL_MULTISAMPLE);


    return true;
}


void draw() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Модельная матрица ---
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, g_modelTranslation);
    model = glm::rotate(model, g_rotationAngleZ, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, g_modelScale);

    // --- Матрица вида (камеры) ---
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    // --- Матрица проекции ---
    int width, height;
    glfwGetFramebufferSize(g_window, &width, &height);
    float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // --- Комбинированные матрицы ---
    glm::mat4 vp = projection * view; // View-Projection для TES
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model))); // Для трансформации нормалей

    // --- Активация шейдера ---
    glUseProgram(g_object.shaderProgram);

    // --- Привязка текстур к текстурным юнитам ---
    glActiveTexture(GL_TEXTURE0); // Активируем текстурный юнит 0
    glBindTexture(GL_TEXTURE_2D, g_object.texture1); // Привязываем текстуру 1
    glUniform1i(g_object.u_Texture1, 0); // Говорим шейдеру использовать юнит 0 для u_texture1

    glActiveTexture(GL_TEXTURE1); // Активируем текстурный юнит 1
    glBindTexture(GL_TEXTURE_2D, g_object.texture2); // Привязываем текстуру 2
    glUniform1i(g_object.u_Texture2, 1); // Говорим шейдеру использовать юнит 1 для u_texture2


    // --- Передача Uniforms ---
    // Матрицы
    glUniformMatrix4fv(g_object.u_Model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(g_object.u_NormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniformMatrix4fv(g_object.u_VP, 1, GL_FALSE, glm::value_ptr(vp));

    // Параметры освещения
    glUniform3fv(g_object.u_LightPos, 1, glm::value_ptr(LIGHT_POS));
    glUniform3fv(g_object.u_ViewPos, 1, glm::value_ptr(cameraPos));
    glUniform3fv(g_object.u_LightColor, 1, glm::value_ptr(LIGHT_COLOR));
    glUniform3fv(g_object.u_AmbientColor, 1, glm::value_ptr(MATERIAL_AMBIENT));
    // glUniform3fv(g_object.u_DiffuseColor, 1, glm::value_ptr(MATERIAL_DIFFUSE)); // Удалено
    glUniform3fv(g_object.u_SpecularColor, 1, glm::value_ptr(MATERIAL_SPECULAR));
    glUniform1f(g_object.u_Shininess, MATERIAL_SHININESS);

    // Параметр смешивания текстур
    glUniform1f(g_object.u_BlendFactor, g_blendFactor);

    // Уровни тесселяции
    if (g_object.u_TessLevelInner != -1) glUniform1f(g_object.u_TessLevelInner, TESS_LEVEL_INNER);
    if (g_object.u_TessLevelOuter != -1) glUniform1f(g_object.u_TessLevelOuter, TESS_LEVEL_OUTER);

    // --- Отрисовка ---
    glBindVertexArray(g_object.vao);
    // Используем GL_PATCHES вместо GL_TRIANGLES, т.к. используем тесселяцию
    glDrawElements(GL_PATCHES, g_object.indexCount, GL_UNSIGNED_INT, NULL);

    // --- Отвязка ресурсов ---
    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0); // Можно сбросить активный юнит
    glBindTexture(GL_TEXTURE_2D, 0); // Отвязать текстуру от юнита 0
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0); // Отвязать текстуру от юнита 1
}


void cleanupApp() {
    // Удаляем шейдерную программу
    if (g_object.shaderProgram != 0) {
        glDeleteProgram(g_object.shaderProgram);
        g_object.shaderProgram = 0;
    }
    // Удаляем буферы вершин и индексов
    if (g_object.vbo != 0) {
        glDeleteBuffers(1, &g_object.vbo);
        g_object.vbo = 0;
    }
    if (g_object.ibo != 0) {
        glDeleteBuffers(1, &g_object.ibo);
        g_object.ibo = 0;
    }
    // Удаляем VAO
    if (g_object.vao != 0) {
        glDeleteVertexArrays(1, &g_object.vao);
        g_object.vao = 0;
    }
    // Удаляем текстуры
    if (g_object.texture1 != 0) {
        glDeleteTextures(1, &g_object.texture1);
        g_object.texture1 = 0;
    }
    if (g_object.texture2 != 0) {
        glDeleteTextures(1, &g_object.texture2);
        g_object.texture2 = 0;
    }
}


bool initOpenGL() {
    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return false;
    }

    // Запрашиваем OpenGL 4.1 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Для macOS
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // Запрашиваем 4x MSAA (мультисемплинг)

    g_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (g_window == NULL) {
        cerr << "Failed to open GLFW window. Check OpenGL version support (4.1 Core required)." << endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);

    // Инициализация GLEW
    glewExperimental = true; // Необходимо для Core Profile
    if (glewInit() != GLEW_OK) {
        cerr << "Failed to initialize GLEW" << endl;
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    // Вывод информации о версии OpenGL
    cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << endl;
    cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

    // Включаем VSync (вертикальную синхронизацию)
    glfwSwapInterval(1);

    // Включаем "залипание" клавиш, чтобы можно было проверять их состояние каждый кадр
    glfwSetInputMode(g_window, GLFW_STICKY_KEYS, GL_TRUE);

    return true;
}

void tearDownOpenGL() {
    if (g_window) {
        glfwDestroyWindow(g_window);
        g_window = nullptr;
    }
    glfwTerminate();
}

// Функция обработки ввода с клавиатуры (для плавных движений и изменений)
void processInput()
{
    float currentCameraSpeed = cameraSpeed * g_deltaTime;
    float currentRotationSpeed = sensitivity * g_deltaTime;
    float currentBlendSpeed = g_blendFactorChangeSpeed * g_deltaTime;

    // Перемещение камеры WASD + Space/Shift
    if (glfwGetKey(g_window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += currentCameraSpeed * cameraFront;
    if (glfwGetKey(g_window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= currentCameraSpeed * cameraFront;
    if (glfwGetKey(g_window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * currentCameraSpeed;
    if (glfwGetKey(g_window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * currentCameraSpeed;
    if (glfwGetKey(g_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += currentCameraSpeed * cameraUp;
    if (glfwGetKey(g_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= currentCameraSpeed * cameraUp;

    // Поворот камеры стрелками
    if (glfwGetKey(g_window, GLFW_KEY_UP) == GLFW_PRESS)
        pitch += currentRotationSpeed;
    if (glfwGetKey(g_window, GLFW_KEY_DOWN) == GLFW_PRESS)
        pitch -= currentRotationSpeed;
    if (glfwGetKey(g_window, GLFW_KEY_LEFT) == GLFW_PRESS)
        yaw -= currentRotationSpeed;
    if (glfwGetKey(g_window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        yaw += currentRotationSpeed;

    // Ограничиваем угол тангажа (pitch)
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // Обновляем вектор cameraFront на основе измененных углов yaw и pitch
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    // Динамическое изменение коэффициента смешивания текстур (PageUp / PageDown)
    if (glfwGetKey(g_window, GLFW_KEY_E) == GLFW_PRESS)
        g_blendFactor += currentBlendSpeed;
    if (glfwGetKey(g_window, GLFW_KEY_Q) == GLFW_PRESS)
        g_blendFactor -= currentBlendSpeed;

    // Ограничиваем коэффициент смешивания в диапазоне [0.0, 1.0]
    g_blendFactor = glm::clamp(g_blendFactor, 0.0f, 1.0f);

}


int main() {
    if (!initOpenGL()) {
        return -1;
    }
    if (!initApp()) {
        cerr << "Failed to initialize application!" << endl;
        cleanupApp();
        tearDownOpenGL();
        return -1;
    }

    g_lastTime = glfwGetTime();

    // Рассчитаем начальный cameraFront на основе установленных yaw/pitch
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    // Главный цикл рендеринга
    while (!glfwWindowShouldClose(g_window)) {
        // Расчет deltaTime
        double currentTime = glfwGetTime();
        g_deltaTime = static_cast<float>(currentTime - g_lastTime);
        g_lastTime = currentTime;

        // Обработка ввода (плавное движение/поворот камеры, изменение смешивания)
        processInput();
        // Обработка событий окна (включая однократные нажатия F и ESC из keyCallback)
        glfwPollEvents();

        // Обновление угла вращения модели для анимации
        if (g_rotate) {
            g_rotationAngleZ += g_objectRotationSpeedRad * g_deltaTime;
            // Ограничение угла, чтобы избежать слишком больших значений (не обязательно)
            if (g_rotationAngleZ > glm::two_pi<float>())
                g_rotationAngleZ -= glm::two_pi<float>();
            else if (g_rotationAngleZ < 0.0f)
                g_rotationAngleZ += glm::two_pi<float>();
        }

        // Отрисовка сцены
        draw();

        // Обмен буферов (показ отрисованного кадра)
        glfwSwapBuffers(g_window);
    }

    // Очистка ресурсов приложения и OpenGL
    cleanupApp();
    tearDownOpenGL();
    return 0;
}