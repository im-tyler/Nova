## Programmatic test of the Scatter PCG pipeline.
##
## Runs on _ready(): creates a SurfaceSampler -> SlopeFilter -> RandomTransform
## -> InstancePlacer chain, executes it, and prints results.  Then quits so
## headless CI can capture the output.
extends Node3D


func _ready() -> void:
	print("=== Scatter PCG Pipeline Test ===")
	print("")

	var plane_mesh_instance: MeshInstance3D = $PlaneMesh

	# --- 1. Test ScatterPoint ---
	print("[1] ScatterPoint")
	var pt := ScatterPoint.new(Vector3(1.0, 2.0, 3.0), Vector3.UP)
	print("  position: ", pt.position)
	print("  normal:   ", pt.normal)
	print("  transform origin: ", pt.transform.origin)
	var pt_copy := pt.duplicate_point()
	print("  duplicate works: ", pt_copy.position == pt.position)
	print("")

	# --- 2. Test SurfaceSampler ---
	print("[2] SurfaceSampler (500 points on PlaneMesh)")
	var sampler := SurfaceSampler.new()
	sampler.point_count = 500
	sampler.seed = 42
	sampler.mesh_instance = plane_mesh_instance

	var empty_input: Array[ScatterPoint] = []
	var sampled_points: Array[ScatterPoint] = sampler.execute(empty_input)
	print("  generated: %d points" % sampled_points.size())

	if sampled_points.size() > 0:
		var first := sampled_points[0]
		print("  first point pos:    ", first.position)
		print("  first point normal: ", first.normal)

		# Verify all points are within the plane bounds (-5 to 5 on X and Z)
		var out_of_bounds := 0
		for p in sampled_points:
			if abs(p.position.x) > 5.1 or abs(p.position.z) > 5.1 or abs(p.position.y) > 0.1:
				out_of_bounds += 1
		print("  out-of-bounds points: %d (should be 0)" % out_of_bounds)
	else:
		print("  ERROR: no points generated")
	print("")

	# --- 3. Test SlopeFilter ---
	print("[3] SlopeFilter (allow all slopes 0-90 deg)")
	var slope_filter := SlopeFilter.new()
	slope_filter.min_angle = 0.0
	slope_filter.max_angle = 90.0
	var filtered_all := slope_filter.execute(sampled_points)
	print("  input: %d  output: %d (should be equal for flat plane)" % [sampled_points.size(), filtered_all.size()])

	# Now test restrictive filter -- flat plane normals are (0,1,0), angle to UP = 0 deg.
	# A filter requiring slope > 10 deg should reject all points on a flat plane.
	print("[3b] SlopeFilter (require slope 10-90 deg -- should reject flat)")
	var slope_filter_steep := SlopeFilter.new()
	slope_filter_steep.min_angle = 10.0
	slope_filter_steep.max_angle = 90.0
	var filtered_steep := slope_filter_steep.execute(sampled_points)
	print("  input: %d  output: %d (should be 0 for flat plane)" % [sampled_points.size(), filtered_steep.size()])
	print("")

	# --- 4. Test RandomTransform ---
	print("[4] RandomTransform")
	var rand_xform := RandomTransform.new()
	rand_xform.offset_range = Vector3(0.1, 0.0, 0.1)
	rand_xform.rotation_range = Vector3(0.0, 360.0, 0.0)
	rand_xform.scale_range = Vector2(0.8, 1.2)
	rand_xform.seed = 99
	var transformed := rand_xform.execute(sampled_points)
	print("  transformed: %d points" % transformed.size())
	if transformed.size() > 0:
		print("  first transform basis x: ", transformed[0].transform.basis.x)
		print("  first transform origin:  ", transformed[0].transform.origin)
	print("")

	# --- 5. Test InstancePlacer ---
	print("[5] InstancePlacer (BoxMesh)")
	var placer := InstancePlacer.new()
	var box := BoxMesh.new()
	box.size = Vector3(0.1, 0.1, 0.1)
	placer.mesh = box
	placer.parent = self

	var placed := placer.execute(transformed)
	print("  placed: %d points" % placed.size())
	if placer.output_node != null:
		print("  MultiMeshInstance3D created: YES")
		print("  multimesh instance_count: %d" % placer.output_node.multimesh.instance_count)
	else:
		print("  MultiMeshInstance3D created: NO (error)")
	print("")

	# --- 6. Test ScatterGraph (linear pipeline) ---
	print("[6] ScatterGraph (full pipeline via ScatterGraph resource)")
	var graph := ScatterGraph.new()

	var sampler2 := SurfaceSampler.new()
	sampler2.point_count = 200
	sampler2.seed = 7
	sampler2.mesh_instance = plane_mesh_instance

	var filter2 := SlopeFilter.new()
	filter2.min_angle = 0.0
	filter2.max_angle = 90.0

	var xform2 := RandomTransform.new()
	xform2.seed = 13

	var placer2 := InstancePlacer.new()
	var sphere := SphereMesh.new()
	sphere.radius = 0.05
	sphere.height = 0.1
	placer2.mesh = sphere
	placer2.parent = self

	graph.add_node(sampler2)
	graph.add_node(filter2)
	graph.add_node(xform2)
	graph.add_node(placer2)

	var start_usec := Time.get_ticks_usec()
	var graph_result := graph.execute()
	var elapsed_ms := (Time.get_ticks_usec() - start_usec) / 1000.0

	print("  graph nodes: %d" % graph.nodes.size())
	print("  final points: %d" % graph_result.size())
	print("  elapsed: %.2f ms" % elapsed_ms)
	if placer2.output_node != null:
		print("  second MultiMeshInstance3D: YES (%d instances)" % placer2.output_node.multimesh.instance_count)
	print("")

	# --- 7. Test disabled node ---
	print("[7] Disabled node passthrough")
	filter2.enabled = false
	var graph_result_no_filter := graph.execute()
	print("  with filter disabled, final points: %d (should still be 200)" % graph_result_no_filter.size())
	filter2.enabled = true
	print("")

	print("=== All tests complete ===")

	# Auto-quit after a brief delay so the render shows briefly
	await get_tree().create_timer(2.0).timeout
	get_tree().quit(0)
