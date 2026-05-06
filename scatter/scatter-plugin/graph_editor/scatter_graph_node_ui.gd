## Visual representation of a single ScatterNode inside the graph editor.
##
## Extends GraphNode to display the node's editable properties and expose
## typed input/output ports. Color-coded by category:
##   green  = generator (SurfaceSampler)
##   yellow = filter    (SlopeFilter)
##   blue   = transform (RandomTransform)
##   red    = output    (InstancePlacer)
@tool
class_name ScatterGraphNodeUI
extends GraphNode

## The underlying ScatterNode resource this UI element represents.
var scatter_node: ScatterNode = null

## Port type constant shared across all graph node UIs so connections
## are restricted to the same logical type (points).
const PORT_TYPE_POINTS := 0

## Category colors.
const COLOR_GENERATOR := Color(0.3, 0.78, 0.35)
const COLOR_FILTER    := Color(0.9, 0.8, 0.2)
const COLOR_TRANSFORM := Color(0.35, 0.6, 0.9)
const COLOR_OUTPUT    := Color(0.9, 0.3, 0.3)

## Unique identifier used to track this node inside the graph editor.
var node_id: int = -1

## Category string for external queries.
var category: String = ""


func _init() -> void:
	resizable = true
	custom_minimum_size = Vector2(220, 80)


## Build the UI for the given ScatterNode resource.  Call once after creation.
func setup(p_scatter_node: ScatterNode, p_node_id: int) -> void:
	scatter_node = p_scatter_node
	node_id = p_node_id

	title = scatter_node.label

	# Determine category and port layout based on type.
	if scatter_node is SurfaceSampler:
		_setup_surface_sampler()
	elif scatter_node is SlopeFilter:
		_setup_slope_filter()
	elif scatter_node is RandomTransform:
		_setup_random_transform()
	elif scatter_node is SplineSampler:
		_setup_spline_sampler()
	elif scatter_node is NoiseFilter:
		_setup_noise_filter()
	elif scatter_node is AlignToNormal:
		_setup_align_to_normal()
	elif scatter_node is InstancePlacer:
		_setup_instance_placer()
	else:
		# Generic fallback -- passthrough
		category = "unknown"
		_add_slot_row("Points In", true, "Points Out", true)


## ---- Per-type setup --------------------------------------------------------

func _setup_surface_sampler() -> void:
	category = "generator"

	# Row 0: output-only slot
	_add_slot_row("", false, "Points", true)
	set_slot(0, false, PORT_TYPE_POINTS, COLOR_GENERATOR,
			 true, PORT_TYPE_POINTS, COLOR_GENERATOR)

	# Editable: point_count
	var pc_spin := SpinBox.new()
	pc_spin.min_value = 1
	pc_spin.max_value = 100000
	pc_spin.step = 1
	pc_spin.value = (scatter_node as SurfaceSampler).point_count
	pc_spin.prefix = "Count: "
	pc_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SurfaceSampler).point_count = int(v)
	)
	add_child(pc_spin)

	# Editable: seed
	var seed_spin := SpinBox.new()
	seed_spin.min_value = 0
	seed_spin.max_value = 999999
	seed_spin.step = 1
	seed_spin.value = (scatter_node as SurfaceSampler).seed
	seed_spin.prefix = "Seed: "
	seed_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SurfaceSampler).seed = int(v)
	)
	add_child(seed_spin)

	_apply_category_color(COLOR_GENERATOR)


func _setup_slope_filter() -> void:
	category = "filter"

	# Row 0: input + output
	_add_slot_row("Points", true, "Points", true)
	set_slot(0, true, PORT_TYPE_POINTS, COLOR_FILTER,
			 true, PORT_TYPE_POINTS, COLOR_FILTER)

	# Editable: min_angle
	var min_spin := SpinBox.new()
	min_spin.min_value = 0.0
	min_spin.max_value = 90.0
	min_spin.step = 0.1
	min_spin.value = (scatter_node as SlopeFilter).min_angle
	min_spin.prefix = "Min: "
	min_spin.suffix = " deg"
	min_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SlopeFilter).min_angle = v
	)
	add_child(min_spin)

	# Editable: max_angle
	var max_spin := SpinBox.new()
	max_spin.min_value = 0.0
	max_spin.max_value = 90.0
	max_spin.step = 0.1
	max_spin.value = (scatter_node as SlopeFilter).max_angle
	max_spin.prefix = "Max: "
	max_spin.suffix = " deg"
	max_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SlopeFilter).max_angle = v
	)
	add_child(max_spin)

	_apply_category_color(COLOR_FILTER)


func _setup_random_transform() -> void:
	category = "transform"

	# Row 0: input + output
	_add_slot_row("Points", true, "Points", true)
	set_slot(0, true, PORT_TYPE_POINTS, COLOR_TRANSFORM,
			 true, PORT_TYPE_POINTS, COLOR_TRANSFORM)

	# Editable: seed
	var seed_spin := SpinBox.new()
	seed_spin.min_value = 0
	seed_spin.max_value = 999999
	seed_spin.step = 1
	seed_spin.value = (scatter_node as RandomTransform).seed
	seed_spin.prefix = "Seed: "
	seed_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as RandomTransform).seed = int(v)
	)
	add_child(seed_spin)

	# Editable: scale_range
	var scale_label := Label.new()
	scale_label.text = "Scale: %.2f - %.2f" % [
		(scatter_node as RandomTransform).scale_range.x,
		(scatter_node as RandomTransform).scale_range.y,
	]
	scale_label.name = "ScaleLabel"
	add_child(scale_label)

	var smin_spin := SpinBox.new()
	smin_spin.min_value = 0.01
	smin_spin.max_value = 10.0
	smin_spin.step = 0.01
	smin_spin.value = (scatter_node as RandomTransform).scale_range.x
	smin_spin.prefix = "Scale Min: "
	smin_spin.value_changed.connect(func(v: float) -> void:
		var rt := scatter_node as RandomTransform
		rt.scale_range.x = v
		var lbl := find_child("ScaleLabel", true, false) as Label
		if lbl:
			lbl.text = "Scale: %.2f - %.2f" % [rt.scale_range.x, rt.scale_range.y]
	)
	add_child(smin_spin)

	var smax_spin := SpinBox.new()
	smax_spin.min_value = 0.01
	smax_spin.max_value = 10.0
	smax_spin.step = 0.01
	smax_spin.value = (scatter_node as RandomTransform).scale_range.y
	smax_spin.prefix = "Scale Max: "
	smax_spin.value_changed.connect(func(v: float) -> void:
		var rt := scatter_node as RandomTransform
		rt.scale_range.y = v
		var lbl := find_child("ScaleLabel", true, false) as Label
		if lbl:
			lbl.text = "Scale: %.2f - %.2f" % [rt.scale_range.x, rt.scale_range.y]
	)
	add_child(smax_spin)

	_apply_category_color(COLOR_TRANSFORM)


func _setup_instance_placer() -> void:
	category = "output"

	# Row 0: input only
	_add_slot_row("Points", true, "", false)
	set_slot(0, true, PORT_TYPE_POINTS, COLOR_OUTPUT,
			 false, PORT_TYPE_POINTS, COLOR_OUTPUT)

	# Info label
	var info := Label.new()
	info.text = "Outputs MultiMeshInstance3D"
	info.add_theme_color_override("font_color", Color(0.7, 0.7, 0.7))
	add_child(info)

	_apply_category_color(COLOR_OUTPUT)


func _setup_spline_sampler() -> void:
	category = "generator"

	# Row 0: output-only slot
	_add_slot_row("", false, "Points", true)
	set_slot(0, false, PORT_TYPE_POINTS, COLOR_GENERATOR,
			 true, PORT_TYPE_POINTS, COLOR_GENERATOR)

	# Editable: point_count
	var pc_spin := SpinBox.new()
	pc_spin.min_value = 1
	pc_spin.max_value = 100000
	pc_spin.step = 1
	pc_spin.value = (scatter_node as SplineSampler).point_count
	pc_spin.prefix = "Count: "
	pc_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SplineSampler).point_count = int(v)
	)
	add_child(pc_spin)

	# Editable: offset_range
	var offset_spin := SpinBox.new()
	offset_spin.min_value = 0.0
	offset_spin.max_value = 100.0
	offset_spin.step = 0.1
	offset_spin.value = (scatter_node as SplineSampler).offset_range
	offset_spin.prefix = "Offset: "
	offset_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SplineSampler).offset_range = v
	)
	add_child(offset_spin)

	# Editable: seed
	var seed_spin := SpinBox.new()
	seed_spin.min_value = 0
	seed_spin.max_value = 999999
	seed_spin.step = 1
	seed_spin.value = (scatter_node as SplineSampler).seed
	seed_spin.prefix = "Seed: "
	seed_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as SplineSampler).seed = int(v)
	)
	add_child(seed_spin)

	_apply_category_color(COLOR_GENERATOR)


func _setup_noise_filter() -> void:
	category = "filter"

	# Row 0: input + output
	_add_slot_row("Points", true, "Points", true)
	set_slot(0, true, PORT_TYPE_POINTS, COLOR_FILTER,
			 true, PORT_TYPE_POINTS, COLOR_FILTER)

	# Editable: noise_scale
	var scale_spin := SpinBox.new()
	scale_spin.min_value = 0.01
	scale_spin.max_value = 100.0
	scale_spin.step = 0.1
	scale_spin.value = (scatter_node as NoiseFilter).noise_scale
	scale_spin.prefix = "Scale: "
	scale_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as NoiseFilter).noise_scale = v
	)
	add_child(scale_spin)

	# Editable: threshold
	var thresh_spin := SpinBox.new()
	thresh_spin.min_value = 0.0
	thresh_spin.max_value = 1.0
	thresh_spin.step = 0.01
	thresh_spin.value = (scatter_node as NoiseFilter).threshold
	thresh_spin.prefix = "Threshold: "
	thresh_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as NoiseFilter).threshold = v
	)
	add_child(thresh_spin)

	# Editable: seed
	var seed_spin := SpinBox.new()
	seed_spin.min_value = 0
	seed_spin.max_value = 999999
	seed_spin.step = 1
	seed_spin.value = (scatter_node as NoiseFilter).seed
	seed_spin.prefix = "Seed: "
	seed_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as NoiseFilter).seed = int(v)
	)
	add_child(seed_spin)

	_apply_category_color(COLOR_FILTER)


func _setup_align_to_normal() -> void:
	category = "transform"

	# Row 0: input + output
	_add_slot_row("Points", true, "Points", true)
	set_slot(0, true, PORT_TYPE_POINTS, COLOR_TRANSFORM,
			 true, PORT_TYPE_POINTS, COLOR_TRANSFORM)

	# Editable: blend_factor
	var blend_spin := SpinBox.new()
	blend_spin.min_value = 0.0
	blend_spin.max_value = 1.0
	blend_spin.step = 0.01
	blend_spin.value = (scatter_node as AlignToNormal).blend_factor
	blend_spin.prefix = "Blend: "
	blend_spin.value_changed.connect(func(v: float) -> void:
		(scatter_node as AlignToNormal).blend_factor = v
	)
	add_child(blend_spin)

	_apply_category_color(COLOR_TRANSFORM)


## ---- Helpers ---------------------------------------------------------------

## Add a child HBoxContainer that acts as a slot row.
## left_label/right_label are shown on each side; enable flags control ports.
func _add_slot_row(left_label: String, _left_enabled: bool,
		right_label: String, _right_enabled: bool) -> void:
	var row := HBoxContainer.new()
	row.size_flags_horizontal = Control.SIZE_EXPAND_FILL

	var left := Label.new()
	left.text = left_label
	left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(left)

	var right := Label.new()
	right.text = right_label
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(right)

	add_child(row)


## Apply a self-modulate tint to visually distinguish the node category.
func _apply_category_color(color: Color) -> void:
	self_modulate = Color(color, 0.85)
