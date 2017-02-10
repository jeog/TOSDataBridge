/*
Copyright (C) 2017 Jonathon Ogden   <jeog.dev@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

package io.github.jeog.tosdatabridge;

/**
 * Pair.java
 *
 * @author Jonathon Ogden
 * @version 0.7
 */
public class Pair<T1,T2>{
    public final T1 first;
    public final T2 second;

    public
    Pair(T1 first, T2 second){
        this.first = first;
        this.second = second;
    }

    public
    Pair(){
        this.first = null;
        this.second = null;
    }

    public String
    toString(){
        return "<" + String.valueOf(first) + "," + String.valueOf(second) + ">";
    }
}