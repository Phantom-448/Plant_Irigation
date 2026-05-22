#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H


#define MAX_PROFILES 10
#define PROFILE_NAME_LEN 32

// Struktur, um alle gefundenen Profilnamen zu speichern
typedef struct {
    char names[MAX_PROFILES][PROFILE_NAME_LEN];
    int count;
} ProfileList_t;

// Öffentliche Funktionen
void scan_profiles_on_sd(void);
ProfileList_t* get_available_profiles(void);
void load_and_activate_profile(const char* filename);

#endif // PROFILE_MANAGER_H