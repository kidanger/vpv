#pragma once

class Sequence;

#define MAX_VARS 10

class EditGUI {
public:
    char editprog[4096];
    float vars[MAX_VARS];
    int nvars;

    EditGUI() : editprog(""), nvars(0)  {
        for (int i = 0; i < MAX_VARS; i++)
            vars[i] = 0;
    }

    void display(Sequence& seq, bool focus);

    void validate(Sequence& seq);

};

