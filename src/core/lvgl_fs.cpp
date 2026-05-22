#include "lvgl_fs.h"
#include <lvgl.h>
#include <LittleFS.h>
#include <Arduino.h>

namespace core {

    static void* lv_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
        String full_path = String("/") + String(path);
        full_path.replace("//", "/"); // Ensure no double slashes

        const char* f_mode = (mode == LV_FS_MODE_WR) ? "w" : "r";
        File* file = new File(); // Allocate on heap so LVGL can hold the pointer
        *file = LittleFS.open(full_path, f_mode);

        if(!(*file)) {
            delete file;
            return nullptr;
        }
        return (void*)file;
    }

    static lv_fs_res_t lv_fs_close(lv_fs_drv_t * drv, void * file_p) {
        File* file = (File*)file_p;
        file->close();
        delete file; // Free the heap allocation
        return LV_FS_RES_OK;
    }

    static lv_fs_res_t lv_fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
        File* file = (File*)file_p;
        *br = file->read((uint8_t*)buf, btr);
        return LV_FS_RES_OK;
    }

    static lv_fs_res_t lv_fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
        File* file = (File*)file_p;
        SeekMode mode = SeekSet;
        if(whence == LV_FS_SEEK_CUR) mode = SeekCur;
        else if(whence == LV_FS_SEEK_END) mode = SeekEnd;

        file->seek(pos, mode);
        return LV_FS_RES_OK;
    }

    static lv_fs_res_t lv_fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
        File* file = (File*)file_p;
        *pos_p = file->position();
        return LV_FS_RES_OK;
    }

    void lvgl_fs_init() {
        static lv_fs_drv_t fs_drv;
        lv_fs_drv_init(&fs_drv);
        fs_drv.letter = 'L'; // Matches the "L:img/..." path in your UI elements
        fs_drv.open_cb = lv_fs_open;
        fs_drv.close_cb = lv_fs_close;
        fs_drv.read_cb = lv_fs_read;
        fs_drv.seek_cb = lv_fs_seek;
        fs_drv.tell_cb = lv_fs_tell;
        lv_fs_drv_register(&fs_drv);

        Serial.println("  [Core FS] LVGL LittleFS bridge registered for drive 'L:'");
    }

} // namespace core
