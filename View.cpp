#include <SFML/Graphics/Texture.hpp>

#include "View.hpp"

View::View()
{
    static int id = 0;
    id++;
    ID = "View " + std::to_string(id);

    zoom = 1.f;
    smallzoomfactor = 30.f;
}

void View::compute(const sf::Texture& tex, sf::Vector2f& u, sf::Vector2f& v) const
{
    float w = tex.getSize().x;
    float h = tex.getSize().y;

    u.x = center.x / w - 1 / (2 * zoom);
    u.y = center.y / h - 1 / (2 * zoom);
    v.x = center.x / w + 1 / (2 * zoom);
    v.y = center.y / h + 1 / (2 * zoom);
}

void View::displaySettings() {
    ImGui::DragFloat("Zoom", &zoom, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::DragFloat("Tooltip zoom factor", &smallzoomfactor, .01f, 0.1f, 300.f, "%g", 2);
    ImGui::DragFloat2("Center", &center.x, 0.f, 100.f);
}

