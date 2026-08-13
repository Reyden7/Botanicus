"""Create transparent import sources from the opaque AI-exported UI plates."""

from pathlib import Path

from PIL import Image


SOURCE = Path(r"C:\Users\quent\Documents\Botanicus\image du jeu\UI\commande\icons_botanicus_separes")
DESTINATION = Path(__file__).with_name("CommandTextureSources")

FILES = {
    "bg_credit.png": "white",
    "bg_niveau.png": "white",
    "bg_eau.png": "black",
    "bg_glace.png": "black",
    "bg_normal.png": "black",
    "bg_ténèbre.png": "black",
    "bg_feu.png": "black",
    "bt_commander_normal.png": "white",
    "bt_commander_pressed.png": "white",
}


def remove_background(image: Image.Image, background: str) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = []
    for red, green, blue, _ in rgba.getdata():
        if background == "white":
            distance = 255 - min(red, green, blue)
        else:
            distance = max(red, green, blue)
        alpha = max(0, min(255, (distance - 3) * 6))
        pixels.append((red, green, blue, alpha))
    rgba.putdata(pixels)
    return rgba


DESTINATION.mkdir(parents=True, exist_ok=True)
for source_name, background in FILES.items():
    source_path = SOURCE / source_name
    destination_name = (
        "bg_tenebre.png" if source_name == "bg_ténèbre.png" else source_name
    )
    remove_background(Image.open(source_path), background).save(
        DESTINATION / destination_name
    )
    print(f"Prepared {destination_name}")
