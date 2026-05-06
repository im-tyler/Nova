extends Node3D

@onready var emitter: TempestEmitter = $TempestEmitter
var fps_timer: float = 0.0

func _ready() -> void:
	# Fountain shooting upward is set via emitter properties in the scene.
	# Add force fields via code:

	# Attractor: pulls particles to the right (+X), positioned at (5, 3, 0)
	emitter.add_attractor(Vector3(5.0, 3.0, 0.0), 12.0, 20.0, 1.0)

	# Vortex: swirls particles around the Y axis, centered above the emitter
	emitter.add_vortex(Vector3(0.0, 4.0, 0.0), Vector3(0.0, 1.0, 0.0), 8.0, 15.0, 0.8)

func _process(delta: float) -> void:
	fps_timer += delta
	if fps_timer >= 1.0:
		fps_timer -= 1.0
		print("FPS: ", Engine.get_frames_per_second())
