#include "unity.h"
#include "loaders/voc_loader.h"
#include "raylib.h"

void setUp(void) {}
void tearDown(void) {}

void test_load_explos1(void)
{
    Sound snd = LoadVOC("assets/EXPLOS1.VOC");
    /* frameCount changes after resampling (11025 → device rate), just check valid */
    TEST_ASSERT_GREATER_THAN(0, snd.frameCount);
    UnloadSound(snd);
}

void test_load_all_12_vocs(void)
{
    static const char *files[] = {
        "assets/EXPLOS1.VOC", "assets/EXPLOS2.VOC", "assets/EXPLOS3.VOC",
        "assets/EXPLOS4.VOC", "assets/EXPLOS5.VOC", "assets/URETHAN.VOC",
        "assets/KILI.VOC",    "assets/APPLAUSE.VOC", "assets/PIKKUPOM.VOC",
        "assets/AARGH.VOC",   "assets/KARJAISU.VOC", "assets/PICAXE.VOC",
    };

    for (int i = 0; i < 12; i++) {
        Sound snd = LoadVOC(files[i]);
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, snd.frameCount, files[i]);
        UnloadSound(snd);
    }
}

void test_load_nonexistent(void)
{
    Sound snd = LoadVOC("assets/NONEXISTENT.VOC");
    TEST_ASSERT_EQUAL(0, snd.frameCount);
}

int main(void)
{
    InitAudioDevice();
    UNITY_BEGIN();
    RUN_TEST(test_load_explos1);
    RUN_TEST(test_load_all_12_vocs);
    RUN_TEST(test_load_nonexistent);
    int result = UNITY_END();
    CloseAudioDevice();
    return result;
}
