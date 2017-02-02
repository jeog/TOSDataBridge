package com.tosdatabridge;

public class Pair<T1,T2>{
    public final T1 first;
    public final T2 second;

    public
    Pair(T1 first, T2 second)
    {
        this.first = first;
        this.second = second;
    }

    public
    Pair()
    {
        this.first = null;
        this.second = null;
    }

    public String
    toString()
    {
        return "<" + String.valueOf(first) + "," + String.valueOf(second) + ">";
    }
}