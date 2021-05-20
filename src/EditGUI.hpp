#pragma once

#include <array>

#include "editors.hpp"

struct Sequence;

#define MAX_VARS 10

class EditGUI {
    float vars[MAX_VARS];
    int nvars;

public:
    std::string editprog;
    EditType edittype;

    EditGUI()
        : nvars(0)
        , editprog()
        , edittype(PLAMBDA)
    {
        for (int i = 0; i < MAX_VARS; i++)
            vars[i] = 0;
    }

    void display(Sequence& seq, bool focus);

    void validate(Sequence& seq);

    bool isEditing() const
    {
        return !editprog.empty();
    }

    std::string getEditorName() const;
};
