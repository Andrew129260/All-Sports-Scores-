#include "pebble.h"
#include "models.h"

int sport_get_icon_res_small(Sport sport) {
    switch (sport){
        case SportNFL:
            return RESOURCE_ID_FOOTBALL_16;
        case SportMLB:
            return RESOURCE_ID_BASEBALL_16;
        case SportNHL:
            return RESOURCE_ID_HOCKEY_16;
        case SportNBA:
            return RESOURCE_ID_BASKETBALL_16;
        case SportMLS:
            return RESOURCE_ID_Mls_16; 
        case SportRugby:
            return RESOURCE_ID_Rugby_16; 
        case SportCricket:
            return RESOURCE_ID_Cricket_16; 
        case SportTennis:
            return RESOURCE_ID_IMAGE_SPORT_TENNIS16;
        case SportAFL:
            return RESOURCE_ID_IMAGE_SPORT_AFL16;
        case SportMMA:
            return RESOURCE_ID_IMAGE_SPORT_MMA16;
        default:
            return RESOURCE_ID_STAR_16;
    }
}

int sport_get_icon_res_large(Sport sport) {
    switch (sport){
        case SportNFL:
            return RESOURCE_ID_FOOTBALL_25;
        case SportMLB:
            return RESOURCE_ID_BASEBALL_25;
        case SportNHL:
            return RESOURCE_ID_HOCKEY_25;
        case SportNBA:
            return RESOURCE_ID_BASKETBALL_25;
        case SportMLS:
            return RESOURCE_ID_Mls_25; 
        case SportRugby:
            return RESOURCE_ID_Rugby_25; 
        case SportCricket:
            return RESOURCE_ID_Cricket_25; 
        case SportTennis:
            return RESOURCE_ID_IMAGE_SPORT_TENNIS25;
        case SportAFL:
            return RESOURCE_ID_IMAGE_SPORT_AFL25;
        case SportMMA:
            return RESOURCE_ID_IMAGE_SPORT_MMA25;
        default:
            return RESOURCE_ID_STAR_25;
    }
}

char* sport_get_name(Sport sport) {
    switch (sport){
        case SportNFL:
            return "NFL";
        case SportMLB:
            return "MLB";
        case SportNHL:
            return "NHL";
        case SportNBA:
            return "NBA";
        case SportMLS:
            return "MLS";
        case SportRugby:
            return "Rugby";
        case SportCricket:
            return "Cricket";
        case SportTennis:
            return "Tennis";
        case SportAFL:
            return "AFL";
        case SportMMA:
            return "MMA";
        default:
            return "Favorites";
    }
}
