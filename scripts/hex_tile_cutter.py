#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
hex_tile_cutter.py — GIMP Python-Fu plugin
Overlay a flat-top staggered hex grid on any image and batch-export
each cell as an individual PNG with a proper hex transparency mask,
plus a JSON tileset descriptor for the game.

Install (Linux):
    cp ~/Documents/Code/raids_of_Umm/scripts/hex_tile_cutter.py ~/.config/GIMP/2.10/plug-ins/
    chmod +x ~/.config/GIMP/2.10/plug-ins/hex_tile_cutter.py

Install (Windows):
    Copy to %APPDATA%\GIMP\2.10\plug-ins\

Usage:
    Filters > Hex Tiles > Add Grid Overlay   — visual reference layer
    Filters > Hex Tiles > Export Tiles        — batch export + JSON

Grid geometry (flat-top staggered columns):
    colStep  = cellW * 3/4          (columns overlap by 1/4)
    odd cols shift DOWN by cellH/2
    circumradius R = cellW / 2
    Ideal cellH  = cellW * sqrt(3)/2  ≈  cellW * 0.866

Hex mask inside test (regular flat-top, circumradius R):
    |dy| <= R*sqrt(3)/2   AND   |dx| + |dy|/sqrt(3) <= R
"""

import math
import os
import json

from gimpfu import *   # noqa: F401,F403 — GIMP injects this namespace

# ── Geometry helpers ──────────────────────────────────────────────────────────

def _hex_center(col, row, cw, ch):
    """Centre of hex cell (col, row) in sheet pixel coordinates."""
    cx = col * cw * 0.75 + cw * 0.5
    cy = row * ch + (col % 2) * ch * 0.5 + ch * 0.5
    return cx, cy


def _hex_verts_flat(cx, cy, R):
    """Flat list [x0,y0, x1,y1, ...] of 6 flat-top hex vertices."""
    pts = []
    for k in range(6):
        a = math.radians(60 * k)
        pts.append(cx + R * math.cos(a))
        pts.append(cy + R * math.sin(a))
    return pts


def _crop_origin(col, row, cw, ch):
    """Top-left pixel of hex (col, row) in the source sheet."""
    px = int(round(col * cw * 0.75))
    py = int(round(row * ch + (col % 2) * ch * 0.5))
    return px, py


# ── Grid overlay ──────────────────────────────────────────────────────────────

def hex_grid_overlay(image, drawable, cell_w, cell_h,
                     r, g, b, opacity):
    """
    Add a semi-transparent hex grid layer over the image.
    Use GIMP's zoom/pan to navigate; use this layer as a picking guide.
    """
    img_w = pdb.gimp_image_width(image)
    img_h = pdb.gimp_image_height(image)
    R     = cell_w / 2.0

    # New transparent layer
    layer = pdb.gimp_layer_new(
        image, img_w, img_h, RGBA_IMAGE, "Hex Grid", opacity, LAYER_MODE_NORMAL_LEGACY
    )
    pdb.gimp_image_insert_layer(image, layer, None, -1)
    pdb.gimp_edit_clear(layer)

    pdb.gimp_context_set_foreground((r, g, b))
    pdb.gimp_context_set_opacity(100)
    pdb.gimp_context_set_brush_size(1.5)
    pdb.gimp_context_set_antialias(True)

    col_step = cell_w * 0.75
    num_cols = int(math.ceil(img_w / col_step)) + 2
    num_rows = int(math.ceil(img_h / cell_h))   + 2

    for col in range(num_cols):
        for row in range(num_rows):
            cx, cy = _hex_center(col, row, cell_w, cell_h)
            verts  = _hex_verts_flat(cx, cy, R)
            # Append first vertex again to close the hexagon
            closed = verts + [verts[0], verts[1]]
            pdb.gimp_pencil(layer, len(closed), closed)

    pdb.gimp_displays_flush()
    pdb.gimp_image_clean_all(image)
    gimp.message(
        "Hex grid overlay added.\n"
        "Cell: {}x{}px  |  Ideal H = W*0.866 = {:.0f}px\n"
        "Use Filters > Hex Tiles > Export Tiles when ready.".format(
            cell_w, cell_h, cell_w * 0.866
        )
    )


# ── Tile export ───────────────────────────────────────────────────────────────

def hex_tile_export(image, drawable, cell_w, cell_h, out_dir, tileset_name):
    """
    Walk the staggered hex grid, crop each cell, apply hex transparency
    mask, save as PNG, and write a JSON tileset descriptor.

    Output layout:
        <out_dir>/<tileset_name>/c{col}_r{row}.png
        <game_root>/data/tilesets/<tileset_name>.json   (if out_dir contains 'assets')
    """
    img_w = pdb.gimp_image_width(image)
    img_h = pdb.gimp_image_height(image)
    R     = cell_w / 2.0

    # Flatten source to a single layer for clean cropping
    flat_img  = pdb.gimp_image_duplicate(image)
    pdb.gimp_image_flatten(flat_img)
    src_layer = pdb.gimp_image_get_active_drawable(flat_img)

    col_step = cell_w * 0.75
    num_cols = int(math.ceil(img_w / col_step)) + 1
    num_rows = int(math.ceil(img_h / cell_h))   + 1

    tile_dir = os.path.join(out_dir, tileset_name)
    os.makedirs(tile_dir, exist_ok=True)

    cells   = []
    skipped = 0

    for col in range(num_cols):
        for row in range(num_rows):
            px0, py0 = _crop_origin(col, row, cell_w, cell_h)

            # Skip cells fully outside the sheet
            if px0 >= img_w or py0 >= img_h:
                skipped += 1
                continue

            # Crop a cell_w × cell_h region (clamped at edges)
            pw = min(cell_w, img_w - px0)
            ph = min(cell_h, img_h - py0)
            if pw <= 0 or ph <= 0:
                skipped += 1
                continue

            # ── Duplicate + crop ──────────────────────────────────────────
            tile_img = pdb.gimp_image_duplicate(flat_img)
            pdb.gimp_image_crop(tile_img, pw, ph, px0, py0)

            # Pad to full cell size if near an edge (transparent fill)
            if pw < cell_w or ph < cell_h:
                pdb.gimp_image_resize(tile_img, cell_w, cell_h, 0, 0)

            tile_layer = pdb.gimp_image_get_active_drawable(tile_img)

            # Ensure alpha channel exists
            if not pdb.gimp_drawable_has_alpha(tile_layer):
                pdb.gimp_layer_add_alpha(tile_layer)

            if pw < cell_w or ph < cell_h:
                pdb.gimp_layer_resize_to_image_size(tile_layer)

            # ── Hex mask: select OUTSIDE the hex, clear it ────────────────
            cx    = cell_w / 2.0
            cy    = cell_h / 2.0
            verts = _hex_verts_flat(cx, cy, R)

            pdb.gimp_image_select_polygon(
                tile_img, CHANNEL_OP_REPLACE, 6, verts
            )
            pdb.gimp_selection_invert(tile_img)
            pdb.gimp_edit_clear(tile_layer)
            pdb.gimp_selection_none(tile_img)

            # ── Save PNG ──────────────────────────────────────────────────
            fname = "c{}_r{}.png".format(col, row)
            fpath = os.path.join(tile_dir, fname)

            pdb.file_png_save(
                tile_img, tile_layer, fpath, fname,
                0,   # interlace
                9,   # compression (max)
                1,   # write bKGD chunk
                1,   # write gAMA chunk
                1,   # write oFFs chunk
                1,   # write pHYs chunk
                1,   # write tIME chunk
            )
            pdb.gimp_image_delete(tile_img)

            cells.append({"col": col, "row": row, "file": fpath})

    pdb.gimp_image_delete(flat_img)

    # ── JSON descriptor ───────────────────────────────────────────────────────
    # Try to infer game root from out_dir (look for 'assets' in path)
    json_dir = None
    parts = os.path.abspath(out_dir).split(os.sep)
    if "assets" in parts:
        root = os.sep.join(parts[:parts.index("assets")])
        json_dir = os.path.join(root, "data", "tilesets")
    if not json_dir:
        json_dir = tile_dir

    os.makedirs(json_dir, exist_ok=True)
    json_path = os.path.join(json_dir, tileset_name + ".json")

    with open(json_path, "w") as f:
        json.dump({
            "name":     tileset_name,
            "cellW":    cell_w,
            "cellH":    cell_h,
            "gridType": "flat-top-staggered-columns",
            "cells":    cells,
        }, f, indent=2)

    gimp.message(
        "Done!\n"
        "Exported {} tiles  ({} skipped — outside sheet)\n"
        "Tiles : {}\n"
        "JSON  : {}".format(len(cells), skipped, tile_dir, json_path)
    )


# ── Plugin registration ───────────────────────────────────────────────────────

register(
    "python-fu-hex-grid-overlay",
    "Add a flat-top staggered hex grid overlay layer",
    "Adds a semi-transparent hex grid layer. Use as a visual guide "
    "before running Export Tiles.",
    "Raids of Umm'Natur", "Raids of Umm'Natur", "2025",
    "<Image>/Filters/Hex Tiles/Add Grid Overlay",
    "*",
    [
        (PF_INT,   "cell_w",  "Cell width (px)  [ideal H = W * 0.866]", 128),
        (PF_INT,   "cell_h",  "Cell height (px)",                        111),
        (PF_INT,   "r",       "Line colour R",                            32),
        (PF_INT,   "g",       "Line colour G",                           220),
        (PF_INT,   "b",       "Line colour B",                           230),
        (PF_FLOAT, "opacity", "Layer opacity %",                          55),
    ],
    [],
    hex_grid_overlay,
)

register(
    "python-fu-hex-tile-export",
    "Export hex tiles with transparency mask + JSON",
    "Walks the staggered flat-top hex grid, crops each cell, applies a "
    "regular hex alpha mask, saves individual PNGs, and writes a JSON "
    "tileset descriptor for the game.",
    "Raids of Umm'Natur", "Raids of Umm'Natur", "2025",
    "<Image>/Filters/Hex Tiles/Export Tiles",
    "*",
    [
        (PF_INT,    "cell_w",       "Cell width (px)  [ideal H = W * 0.866]", 128),
        (PF_INT,    "cell_h",       "Cell height (px)",                        111),
        (PF_STRING, "out_dir",      "Output base directory",
                    "assets/textures/terrain"),
        (PF_STRING, "tileset_name", "Tileset name (subfolder + JSON key)",
                    "my_terrain"),
    ],
    [],
    hex_tile_export,
)

main()
