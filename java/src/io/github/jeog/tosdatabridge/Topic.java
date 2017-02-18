package io.github.jeog.tosdatabridge;

/**
 * Enumerated object containing all the valid 'Topics' the Engine/TOS accepts.
 *
 * @author Jonathon Ogden
 * @version 0.7
 */
public enum Topic {
    VOL_DIFF ("VOL_DIFF"),
    AX ("AX"),
    BACK_VOL ("BACK_VOL"),
    LOW ("LOW"),
    PE ("PE"),
    HTB_ETB ("HTB_ETB"),
    BIDX ("BIDX"),
    CALL_VOLUME_INDEX ("CALL_VOLUME_INDEX"),
    NET_CHANGE ("NET_CHANGE"),
    EPS ("EPS"),
    OPTION_VOLUME_INDEX ("OPTION_VOLUME_INDEX"),
    STRIKE ("STRIKE"),
    MARKET_CAP ("MARKET_CAP"),
    MARK_PERCENT_CHANGE ("MARK_PERCENT_CHANGE"),
    FRONT_VOL ("FRONT_VOL"),
    YIELD ("YIELD"),
    STRENGTH_METER ("STRENGTH_METER"),
    ASKX ("ASKX"),
    DIV_FREQ ("DIV_FREQ"),
    LX ("LX"),
    DESCRIPTION ("DESCRIPTION"),
    EX_DIV_DATE ("EX_DIV_DATE"),
    SHARES ("SHARES"),
    BETA ("BETA"),
    EXTRINSIC ("EXTRINSIC"),
    SYMBOL ("SYMBOL"),
    OPEN ("OPEN"),
    MARK_CHANGE ("MARK_CHANGE"),
    BID_SIZE ("BID_SIZE"),
    VOLUME ("VOLUME"),
    WEIGHTED_BACK_VOL ("WEIGHTED_BACK_VOL"),
    LAST ("LAST"),
    LAST_SIZE ("LAST_SIZE"),
    BID ("BID"),
    MRKT_MKR_MOVE ("MRKT_MKR_MOVE"),
    PUT_CALL_RATIO ("PUT_CALL_RATIO"),
    RHO ("RHO"),
    PERCENT_CHANGE ("PERCENT_CHANGE"),
    HIGH52 ("52HIGH"),
    MT_NEWS ("MT_NEWS"),
    VOL_INDEX ("VOL_INDEX"),
    GAMMA ("GAMMA"),
    LOW52 ("52LOW"),
    PUT_VOLUME_INDEX ("PUT_VOLUME_INDEX"),
    DIV ("DIV"),
    ASK ("ASK"),
    DELTA ("DELTA"),
    NULL_TOPIC ("NULL_TOPIC"), // <-- DONT USE
    INTRINSIC ("INTRINSIC"),
    BACK_EX_MOVE ("BACK_EX_MOVE"),
    VEGA ("VEGA"),
    BX ("BX"),
    EXPIRATION ("EXPIRATION"),
    MARK ("MARK"),
    LASTX ("LASTX"),
    THETA ("THETA"),
    FRONT_EX_MOVE ("FRONT_EX_MOVE"),
    BA_SIZE ("BA_SIZE"),
    FX_PAIR ("FX_PAIR"),
    ASK_SIZE ("ASK_SIZE"),
    IMPL_VOL ("IMPL_VOL"),
    EXCHANGE ("EXCHANGE"),
    EX_MOVE_DIFF ("EX_MOVE_DIFF"),
    HIGH ("HIGH"),
    OPEN_INT ("OPEN_INT");

    public final String val;
    Topic(String t){
        this.val = t;
    }

    /* package-private */
    static Topic
    toEnum(String val){
        /* 2 cases where .name() != .val */
        if(val.equals("52HIGH")) {
            return HIGH52;
        }else if(val.equals("52LOW")) {
            return LOW52;
        }else {
            return valueOf(val);
        }
    }
}
