# Map and story handoff

One challenge that remains is moving from a computer world to a fantasy world
with verisimilitude. The challenge is to build a world that makes the demo feel
like a place—not through good sprites, but through design.

Geography, distances, routes, settlements, resources, dangers and history need
to fit together. The map and story should be parts of that design: a named place
matters to the journey, a story beat gives the player a reason to travel or make
a choice, and exploration reveals why the place is the way it is. Avoid a map
that feels like a board of unrelated fights or story scenes that could happen
anywhere.

## Current slice

The Old Passage map has radius 20, 1,261 hexes and 109 landmarks. Five named wolf
packs surround Varenhold; their armies total 94 wolves. The opening quest tracks
wolves and packs remaining. Clearing all five and ending the day brings Ushari
to Varenhold, where she asks `[name], where is your brother?` The name marker is
reserved for future commander-name substitution.

The latest map expansion added eight guard camps, eight caches and four resource
sites. These provide exploration and rewards, but most do not yet advance the
story. Treat those frontier locations as candidates for the next story pass,
not as finished narrative content.

## Direction for the next pass

- Give each major region a story purpose and an objective the player can act on
  there. Use landmarks, encounters, dialogue, journal entries and map changes to
  deliver that purpose together.
- Let travel order shape discovery. Reveal Aldren's trail through evidence at
  places the player visits, while keeping alternate routes and optional sites
  viable.
- Tie important discoveries to the place that contains them. A clue should name
  or change a landmark, update the journal, and point toward a meaningful next
  destination.
- Let player actions change the map and the story state: camps clear, routes
  open, characters move or disappear, and objectives update as those changes
  happen.
- Keep ordinary resource pickups and combat sites optional unless they support
  a clear story or progression purpose. Not every hex needs dialogue.

For Stage 1, the immediate opportunity is to connect a few frontier landmarks
to Aldren's search and House Rook's border, so the enlarged map develops the
existing plot instead of adding travel for its own sake. Preserve the current
five-pack opening and its completion gate while doing so.

See [Demo flow](demo_flow.md) for the beat-by-beat player context and existing
story constraints; see [Map density](map_density.md) for encounter and landmark
spacing guidance.
